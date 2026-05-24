#include "app_input.h"
#include "harti_config.h"
#include "harti_temp.h"
#include "face_api.h"
#include "sprite_registry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "input";

static void input_task(void *arg) {
    (void)arg;

    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    int sprite_count = sprite_registry_count();
    ESP_LOGI(TAG, "BOOT button monitor, %d sprites available", sprite_count);

    int press_cnt = 0;
    bool was_pressed = false;
    int idx = 0;
    int tick = 0;

    while (1) {
        int gpio_level = gpio_get_level(GPIO_NUM_0);
        int adc_raw = temp_read_raw();

        bool is_pressed = (gpio_level == 0) && (adc_raw < BUTTON_PRESS_THRESH);

        tick++;
        if (tick >= 33) {
            tick = 0;
            ESP_LOGI(TAG, "GPIO=%d ADC=%d pressed=%d",
                     gpio_level, adc_raw, is_pressed);
        }

        if (is_pressed) {
            press_cnt++;
            if (press_cnt >= BUTTON_DEBOUNCE_CNT && !was_pressed) {
                was_pressed = true;
                idx = (idx + 1) % sprite_count;
                const sprite_set_t *sprite = sprite_registry_get(idx);
                face_set_sprite(sprite);
                ESP_LOGI(TAG, "→ sprite[%d/%d] %s",
                         idx, sprite_count, sprite->name);
            }
        } else if (!is_pressed && (adc_raw > BUTTON_RELEASE_THRESH || gpio_level == 1)) {
            press_cnt = 0;
            was_pressed = false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

void input_start(void) {
    xTaskCreate(input_task, "input", BUTTON_TASK_STACK,
                NULL, BUTTON_TASK_PRIO, NULL);
}
