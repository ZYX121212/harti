# ESP32-S3 Board GPIO Resources

## Pinout & Harti Usage

| Pin | GPIO | Functions | Harti Usage |
|-----|------|-----------|-------------|
| 1 | - | 3V3 | Power |
| 2 | - | 3V3 | Power |
| 3 | - | RST | Reset |
| 4 | GPIO4 | ADC1_3, TOUCH4, RTC | - |
| 5 | GPIO5 | ADC1_4, TOUCH5, RTC | - |
| 6 | GPIO6 | ADC1_5, TOUCH6, RTC | - |
| 7 | GPIO7 | ADC1_6, TOUCH7, RTC | - |
| 8 | GPIO2 | JTAG, ADC1_2, TOUCH3, RTC | - |
| 9 | GPIO15 | ADC2_4, XTAL_32K_N, U0CTS, RTC | - |
| 10 | GPIO16 | ADC2_5, XTAL_32K_P, U0RTS, RTC | - |
| 11 | GPIO2 | JTAG, ADC1_2, TOUCH3, RTC | - |
| 12 | GPIO46 | LOG | - |
| 13 | GPIO9 | FSPIHD, SUBSPIHD, ADC1_8, TOUCH9, RTC | **I2C SCL** (MPU6050 IMU) |
| 14 | GPIO10 | FSPICS0, SUBSPI_CS0, FSPIIO4, ADC1_9, TOUCH10, RTC | - |
| 15 | GPIO11 | FSPID, SUBSPID, FSPIIO5, ADC2_0, TOUCH11, RTC | - |
| 16 | GPIO12 | FSPICLK, SUBSPICLK, FSPIIO6, ADC2_1, TOUCH12, RTC | - |
| 17 | GPIO13 | FSPIQ, SUBSPIQ, FSPIIO7, ADC2_2, TOUCH13, RTC | - |
| 18 | GPIO14 | FSPIWP, SUBSPIWP, FSPIIDQS, ADC2_3, TOUCH14, RTC | - |
| 19 | - | GND | Ground |
| 20 | GPIO43 | U0TXD, CLK_OUT1 | - |
| 21 | GPIO44 | U0RXD, CLK_OUT2 | - |
| 22 | GPIO1 | RTC, TOUCH1, ADC1_0 | **TOUCH0** (head touch) |
| 23 | GPIO2 | RTC, TOUCH2, ADC1_1 | - |
| 24 | GPIO42 | MTMS | **LCD RST** (GC9A01 reset) |
| 25 | GPIO41 | MTDI, CLK_OUT1 | **LCD DC** (GC9A01 data/cmd) |
| 26 | GPIO40 | MTDO, CLK_OUT2 | **LCD CS** (GC9A01 SPI CS) |
| 27 | GPIO39 | MTCK, CLK_OUT3, SUBSPI_CS1 | - |
| 28 | GPIO38 | FSPIWP, SUBSPIWP | - |
| 29 | GPIO37 | FSPIQ, SUBSPIQ | - |
| 30 | GPIO36 | SPIIO7, FSPICLK, SUBSPICLK | **LCD SCK** (GC9A01 SPI clock) |
| 31 | GPIO35 | SPIIO6, FSPID, SUBSPID | **LCD SDA** (GC9A01 SPI MOSI) |
| 32 | GPIO0 | BOOT | **ADC1_CH0** (NTC temp) |
| 33 | GPIO45 | VSPI | - |
| 34 | GPIO48 | SPICLK_L_P, RGB LED | - |
| 35 | GPIO47 | SPICLK_L_N | - |
| 36 | GPIO21 | RTC | - |
| 37 | GPIO20 | USB_D+, RTC, U1CTS, ADC2_9, CLK_OUT1 | - |
| 38 | GPIO19 | USB_D-, RTC, U1RTS, ADC2_8, CLK_OUT2 | - |
| 39 | - | GND | Ground |
| 40 | - | GND | Ground |

### Additional Connections

| GPIO | Functions | Harti Usage |
|------|-----------|-------------|
| GPIO8 | ADC1_7, TOUCH8, RTC | **I2C SDA** (MPU6050 IMU) |

## Summary of Harti GPIO Assignments

| GPIO | Function | Connected To | Defined In |
|------|----------|-------------|------------|
| GPIO0 | ADC1_CH0 | NTC thermistor (temperature) | `components/harti_temp/harti_temp.c:15-16` |
| GPIO1 | TOUCH0 | Head touch copper foil | `main/app_sensors.c:18` |
| GPIO8 | I2C SDA | MPU6050 IMU | `main/app_sensors.c:17` |
| GPIO9 | I2C SCL | MPU6050 IMU | `main/app_sensors.c:16` |
| GPIO35 | SPI MOSI | GC9A01 LCD (SDA) | `components/gc9a01/gc9a01.c:13` |
| GPIO36 | SPI SCK | GC9A01 LCD (SCK) | `components/gc9a01/gc9a01.c:14` |
| GPIO40 | SPI CS | GC9A01 LCD (CS) | `components/gc9a01/gc9a01.c:17` |
| GPIO41 | GPIO Out | GC9A01 LCD (DC) | `components/gc9a01/gc9a01.c:16` |
| GPIO42 | GPIO Out | GC9A01 LCD (RST) | `components/gc9a01/gc9a01.c:15` |

Notes:
- **Backlight (BLK)**: disabled in code (`PIN_BLK = -1`), LCD backlight hardwired to VCC
- **SPI peripheral**: `SPI2_HOST` at 10 MHz, 3-wire (MOSI only, no MISO)
- **I2C peripheral**: `I2C_NUM_0`, internal pull-ups enabled
- **IMU**: MPU6050 driver (`components/harti_imu/harti_imu.c`), I2C addr 0x68, accel ±8g / gyro ±500dps / 100Hz ODR
- **Touch**: only GPIO1 (TOUCH0) is used; HARDWARE.md references GPIO2/GPIO3 for face touch but these are not implemented in code
