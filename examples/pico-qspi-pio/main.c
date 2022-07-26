#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

#include "qspi.pio.h"

const uint32_t sio[] = {8, 9, 10, 11};
const uint32_t clk = 12;
const uint32_t ce[] = {13, 14};

const uint32_t mosi = 8;
const uint32_t miso = 9;

uint8_t spi_transfer(uint8_t b) {
    while (pio_sm_is_tx_fifo_full(pio0, 0));
    *(&pio0->txf[0]) = (uint32_t) b << 24;
    while (pio_sm_is_rx_fifo_empty(pio0, 0));
    return *(&pio0->rxf[0]);
}

uint32_t spi_transfer32(uint32_t b) {
    while (pio_sm_is_tx_fifo_full(pio0, 1));
    *(&pio0->txf[1]) = b;
    while (pio_sm_is_rx_fifo_empty(pio0, 1));
    return *(&pio0->rxf[1]);
}

void ram_spi_reset() {
    // reset enable
    gpio_clr_mask(0x3u << ce[0]);
    spi_transfer(0x66);
    gpio_set_mask(0x3u << ce[0]);

    // reset
    gpio_clr_mask(0x3u << ce[0]);
    spi_transfer(0x99);
    gpio_set_mask(0x3u << ce[0]);

    // delay to allow reset
    sleep_ms(10);
}

uint64_t ram_spi_readid() {
    /*uint64_t b = 0;
    spi_transfer(0x9F);
    spi_transfer(0x00);
    spi_transfer(0x00);
    spi_transfer(0x00);
    for (uint i = 0; i < 8; i++) {
        b = (b << 8) | spi_transfer(0x00);
    }
    return b;*/

    uint64_t id = 0;
    pio_sm_set_enabled(pio0, 1, true);
    spi_transfer32(0x9F000000);
    id = spi_transfer32(0);
    id = (id << 32) | spi_transfer32(0);
    pio_sm_set_enabled(pio0, 1, false);
    return id;
}

void ram_spi_toggle_burst_length() {
    spi_transfer(0xC0);
}

// read block of data
void ram_spi_fast_read(uint32_t address, uint8_t* data, size_t length) {
    spi_transfer(0x0B);
    spi_transfer((address >> 16) & 0xff);
    spi_transfer((address >> 8) & 0xff);
    spi_transfer((address & 0xff));

    // 8-clock wait
    (void) spi_transfer(0x00);

    // read data
    while (length--) {
        *(data++) = spi_transfer(0x00);
    }
}

// write block of data
void ram_spi_fast_write(uint32_t address, uint8_t* data, size_t length) {
    spi_transfer(0x02);
    spi_transfer((address >> 16) & 0xff);
    spi_transfer((address >> 8) & 0xff);
    spi_transfer((address & 0xff));
    while (length--) {
        spi_transfer(*(data++));
    }
}

int main() {
    stdio_init_all();

    // setup spi chip enables
    gpio_init_mask(3u << ce[0]);
    gpio_set_dir_masked(3u << ce[0], 3u << ce[0]);
    gpio_put_masked(3u << ce[0], 3u << ce[0]);

    // Load PIO for bus control
    uint offset = pio_add_program(pio0, &spi_program);
    spi_program_init(pio0, 0, offset, 8, 2.f, clk, mosi, miso);
    pio_sm_set_enabled(pio0, 0, true);

    offset = pio_add_program(pio0, &spi_program);
    spi_program_init(pio0, 1, offset, 32, 2.f, clk, mosi, miso);
    pio_sm_set_enabled(pio0, 1, false);

    // reset the rams
    ram_spi_reset();

    // read id
    for (uint i = 0; i < 2; i++) {
        gpio_put(ce[i], false);
        uint64_t id = ram_spi_readid();
        gpio_put(ce[i], true);

        printf("ram[%u] vendor/kgd = 0x%04x, eid = 0x%012llx\n",
                i, (id >> 48) & 0xffff, id & ((1llu << 48llu) - 1llu));
    }

    // enter qspi mode
    //gpio_clr_mask(0x3u << ce[0]);
    //ram_qspi_enable();
    //gpio_set_mask(0x3u << ce[0]);

    // toggle burst length
    gpio_clr_mask(0x3u << ce[0]);
    ram_spi_toggle_burst_length();
    gpio_set_mask(0x3u << ce[0]);

    // write some data
    uint8_t data[64] = {0};
    uint8_t data_in[64] = {0};
    for (uint i = 0; i < 64; i++) {
        data[i] = i;
    }

    gpio_put(ce[0], false);
    ram_spi_fast_write(0, data, 32);
    gpio_put(ce[0], true);

    gpio_put(ce[1], false);
    ram_spi_fast_write(0, data + 32, 32);
    gpio_put(ce[1], true);

    // exit qspi mode
    //gpio_clr_mask(0x3u << ce[0]);
    //ram_qspi_disable();
    //gpio_set_mask(0x3u << ce[0]);

    gpio_put(ce[0], false);
    ram_spi_fast_read(0, data_in, 32);
    gpio_put(ce[0], true);

    gpio_put(ce[1], false);
    ram_spi_fast_read(0, data_in + 32, 32);
    gpio_put(ce[1], true);

    // dump all data
    for (uint i = 0; i < 64; i++) {
        printf("data_in[0x%02x] = %02x\n", i, data_in[i]);
    }

    // TODO: something useful
    while (true) {
        sleep_ms(1000);
    }
}