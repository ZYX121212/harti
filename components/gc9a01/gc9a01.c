#include "gc9a01.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GC9A01";

// 引脚定义 (根据实际排线连接)
#define PIN_SDA  35   // SPI MOSI
#define PIN_SCK  36   // SPI SCK
#define PIN_RES  42   // LCD RST
#define PIN_DC   41   // LCD DC (数据/命令)
#define PIN_CS   40   // LCD CS (片选)
#define PIN_BLK  -1   // 无独立背光引脚 (VCC 供电即亮)

#define SPI_HOST  SPI2_HOST
#define SPI_FREQ  10000000  // 10MHz (排线连接降速确保稳定)

static spi_device_handle_t spi_dev;

static void gc9a01_send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_data[0] = cmd,
        .flags = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(spi_dev, &t);
}

static void gc9a01_send_data(const uint8_t *data, size_t len)
{
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi_dev, &t);
}

void gc9a01_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    gc9a01_send_cmd(0x2A); // Column Address Set
    uint8_t buf[4] = {
        (x0 >> 8) & 0xFF, x0 & 0xFF,
        (x1 >> 8) & 0xFF, x1 & 0xFF
    };
    gc9a01_send_data(buf, 4);

    gc9a01_send_cmd(0x2B); // Row Address Set
    buf[0] = (y0 >> 8) & 0xFF;
    buf[1] = y0 & 0xFF;
    buf[2] = (y1 >> 8) & 0xFF;
    buf[3] = y1 & 0xFF;
    gc9a01_send_data(buf, 4);

    gc9a01_send_cmd(0x2C); // Memory Write
}

void gc9a01_send_pixels(const uint16_t *pixels, size_t len)
{
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 16,
        .tx_buffer = pixels,
    };
    spi_device_polling_transmit(spi_dev, &t);
}

// GC9A01 初始化序列
static const uint8_t init_seq[] = {
    0xEF, 0,
    0xEB, 1, 0x14,
    0xFE, 0,
    0xEF, 0,
    0xEB, 1, 0x14,
    0x84, 1, 0x40,
    0x85, 1, 0xFF,
    0x86, 1, 0xFF,
    0x87, 1, 0xFF,
    0x88, 1, 0x0A,
    0x89, 1, 0x21,
    0x8A, 1, 0x00,
    0x8B, 1, 0x80,
    0x8C, 1, 0x01,
    0x8D, 1, 0x01,
    0x8E, 1, 0xFF,
    0x8F, 1, 0xFF,
    0xB6, 2, 0x00, 0x20,
    0x36, 1, 0x08,  // 内存访问控制: 从上到下, 从左到右, RGB
    0x3A, 1, 0x05,  // 像素格式: 16-bit RGB565
    0x90, 4, 0x08, 0x08, 0x08, 0x08,
    0xBD, 1, 0x06,
    0xBC, 1, 0x00,
    0xFF, 3, 0x60, 0x01, 0x04,
    0xC3, 1, 0x13,
    0xC4, 1, 0x13,
    0xC9, 1, 0x22,
    0xBE, 1, 0x11,
    0xE1, 2, 0x10, 0x0E,
    0xDF, 3, 0x21, 0x0c, 0x02,
    0xF0, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    0xF1, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    0xF2, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    0xF3, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    0xED, 2, 0x1B, 0x0B,
    0xAE, 1, 0x77,
    0xCD, 1, 0x63,
    0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
    0xE8, 1, 0x34,
    0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
    0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
    0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
    0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
    0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
    0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
    0x98, 2, 0x3e, 0x07,
    0x35, 0,  // 开启 Tearing Effect Line
    0x21, 0,  // 显示反转开启
    0x11, 0,  // 退出睡眠
    0x00,  // 结束标记
};

void gc9a01_init(void)
{
    ESP_LOGI(TAG, "Initializing GC9A01...");

    // GPIO 初始化 (DC, RES, CS)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RES) | (1ULL << PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(PIN_CS, 1);  // 片选初始高

    // SPI 初始化
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SDA,
        .sclk_io_num = PIN_SCK,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_WIDTH * 2 * 2, // 两行
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_FREQ,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST, &dev_cfg, &spi_dev));

    // 硬件复位
    gpio_set_level(PIN_RES, 0);
    esp_rom_delay_us(10000);
    gpio_set_level(PIN_RES, 1);
    esp_rom_delay_us(100000);

    // 发送初始化序列
    int i = 0;
    while (init_seq[i] != 0x00) {
        uint8_t cmd = init_seq[i++];
        uint8_t len = init_seq[i++];
        gc9a01_send_cmd(cmd);
        if (len > 0) {
            gc9a01_send_data(&init_seq[i], len);
            i += len;
        }
    }

    // 等待
    esp_rom_delay_us(120000);

    // 开启显示
    gc9a01_send_cmd(0x29);

    ESP_LOGI(TAG, "GC9A01 initialized");
}
