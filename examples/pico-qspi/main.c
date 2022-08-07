#include <stdio.h>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

const uint32_t sio[] = {8, 9, 10, 11};
const uint32_t clk = 12;
const uint32_t ce[] = {13, 14};

const uint32_t mosi = 8;
const uint32_t miso = 9;

const uint32_t qspi_mask = 0x7F00;

void spi_write(uint8_t b) {
    uint i = 7;
    do {
        gpio_put(mosi, !!((b >> i) & 1));
        gpio_put(clk, true);
        gpio_put(clk, false);
    } while(i--);
}

uint8_t spi_read() {
    uint8_t b = 0;
    for (uint i = 0; i < 8; i++) {
        b = (b << 1) | (gpio_get(miso) ? 0x01 : 0x00);
        gpio_put(clk, true);
        gpio_put(clk, false);
    }
    return b;
}

void ram_spi_reset() {
    // reset enable
    gpio_clr_mask(0x3u << ce[0]);
    spi_write(0x66);
    gpio_set_mask(0x3u << ce[0]);

    // reset
    gpio_clr_mask(0x3u << ce[0]);
    spi_write(0x99);
    gpio_set_mask(0x3u << ce[0]);

    // delay to allow reset
    sleep_ms(10);
}

uint64_t ram_spi_readid() {
    uint64_t b = 0;
    spi_write(0x9F);
    spi_write(0x00);
    spi_write(0x00);
    spi_write(0x00);
    for (uint i = 0; i < 8; i++) {
        b = (b << 8) | spi_read();
    }
    return b;
}

void ram_spi_toggle_burst_length() {
    spi_write(0xC0);
}

// read block of data
void ram_spi_fast_read(uint32_t address, uint8_t* data, size_t length) {
    spi_write(0x0B);
    spi_write((address >> 16) & 0xff);
    spi_write((address >> 8) & 0xff);
    spi_write((address & 0xff));

    // 8-clock wait
    (void) spi_read();

    // read data
    while (length--) {
        *(data++) = spi_read();
    }
}

// write block of data
void ram_spi_fast_write(uint32_t address, uint8_t* data, size_t length) {
    spi_write(0x02);
    spi_write((address >> 16) & 0xff);
    spi_write((address >> 8) & 0xff);
    spi_write((address & 0xff));
    while (length--) {
        spi_write(*(data++));
    }
}

void qspi_write(uint8_t b) {
    uint16_t d = (uint16_t) b << 8;

    gpio_put_masked(0xF00, d >> 4);
    gpio_put(clk, true);
    gpio_put(clk, false);

    gpio_put_masked(0xF00, d);
    gpio_put(clk, true);
    gpio_put(clk, false);
}

void ram_qspi_enable() {
    spi_write(0x35);
    // qspi mode default to master driven io
    gpio_set_dir_masked(0xF00, 0xF00);
}

void ram_qspi_disable() {
    gpio_put_masked(0xF00, 0xF00);
    gpio_put(clk, true);
    gpio_put(clk, false);

    gpio_put_masked(0xF00, 0x500);
    gpio_put(clk, true);
    gpio_put(clk, false);

    // spi mode sio[3..2] are master inputs, miso is input, mosi is output
    gpio_set_dir_masked(0xF00, 0x100);
}

void ram_qspi_reset() {
    // reset enable
    gpio_clr_mask(0x3u << ce[0]);
    qspi_write(0x66);
    gpio_set_mask(0x3u << ce[0]);

    // reset
    gpio_clr_mask(0x3u << ce[0]);
    qspi_write(0x99);
    gpio_set_mask(0x3u << ce[0]);

    // spi mode sio[3..2] are master inputs, miso is input, mosi is output
    gpio_set_dir_masked(0xF00, 0x100);
    sleep_ms(10);
}

void ram_qspi_toggle_burst_length() {
    qspi_write(0xC0);
}

// read block of data
void ram_qspi_fast_read(uint32_t address, uint8_t* data, size_t length) {
    qspi_write(0xEB);
    qspi_write((address >> 16) & 0xff);
    qspi_write((address >> 8) & 0xff);
    qspi_write((address & 0xff));

    gpio_set_dir_masked(0xF00, 0x000);

    // 6-clock wait
    for (uint i = 0; i < 6; i++) {
        gpio_put(clk, true);
        gpio_put(clk, false);
    }

    // read data
    for (uint i = 0; i < length; i++) {
        // upper nibble
        asm volatile("nop\n");
        uint32_t a = (gpio_get_all() >> 8) & 0x0Fu;
        gpio_put(clk, true);
        gpio_put(clk, false);

        // lower nibble
        asm volatile("nop\n");
        uint32_t b = (gpio_get_all() >> 8) & 0x0Fu;
        gpio_put(clk, true);
        gpio_put(clk, false);

        // store data
        *(data++) = (a << 4) | b;
    }
    gpio_set_dir_masked(0xF00, 0xF00);
}

// write block of data
void ram_qspi_fast_write(uint32_t address, uint8_t* data, size_t length) {
    qspi_write(0x38);
    qspi_write((address >> 16) & 0xff);
    qspi_write((address >> 8) & 0xff);
    qspi_write((address & 0xff));
    while (length--) {
        qspi_write(*(data++));
    }
}

int main() {
    stdio_init_all();

    // setup gpio for qspi (ce0, ce1, clk, mosi are outputs, ce0 and ce1 drive high, drive clk low)
    gpio_init_mask(qspi_mask);
    gpio_set_dir_masked(qspi_mask, 0x7100u);
    gpio_put_masked(qspi_mask, 0x6000u);

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
    gpio_clr_mask(0x3u << ce[0]);
    ram_qspi_enable();
    gpio_set_mask(0x3u << ce[0]);

    // toggle burst length
    gpio_clr_mask(0x3u << ce[0]);
    ram_qspi_toggle_burst_length();
    gpio_set_mask(0x3u << ce[0]);

    // write some data
    uint8_t data[64] = {0};
    uint8_t data_in[64] = {0};
    for (uint i = 0; i < 64; i++) {
        data[i] = i;
    }

    gpio_put(ce[0], false);
    ram_qspi_fast_write(0, data, 32);
    gpio_put(ce[0], true);

    gpio_put(ce[1], false);
    ram_qspi_fast_write(0, data + 32, 32);
    gpio_put(ce[1], true);

    gpio_put(ce[0], false);
    ram_qspi_fast_read(0, data_in, 32);
    gpio_put(ce[0], true);

    gpio_put(ce[1], false);
    ram_qspi_fast_read(0, data_in + 32, 32);
    gpio_put(ce[1], true);

    // exit qspi mode
    gpio_clr_mask(0x3u << ce[0]);
    ram_qspi_disable();
    gpio_set_mask(0x3u << ce[0]);

    // dump all data
    for (uint i = 0; i < 64; i++) {
        printf("data_in[0x%02x] = %02x\n", i, data_in[i]);
    }

    // TODO: something useful
    while (1) {
        tight_loop_contents();
    }
}