# PMOD-PSRAM

PMOD containing two QSPI PSRAM (pseudo-static ram) modules. Provides up to 16 megabytes of memory using standard sized 8Mx8 modules (such as APS6404 or ESP32-PSRAM64H).

![Assembled](/Assets/assembled.jpg)

### Pinout

Pinout follows a Pmod Type 1A pinout

| Pin | Function    |
| --- | ----------- |
| 1   | CS0_n       |
| 2   | SIO2        |
| 3   | x           |
| 4   | SCLK        |
| 5   | GND         |
| 6   | 3.3V        |
| 7   | SIO1 (MISO) |
| 8   | SIO3        |
| 9   | SIO0 (MOSI) |
| 10  | CS1_n       |
| 11  | GND         |
| 12  | 3.3V        |

### References

[Pmod Specification v1.3](https://digilent.com/reference/_media/reference/pmod/pmod-interface-specification-1_3_0.pdf)

[ESP32-PSRAM64H Datasheet](https://www.espressif.com/sites/default/files/documentation/esp-psram64_esp-psram64h_datasheet_en.pdf)

### License

CC-BY-SA 4.0
