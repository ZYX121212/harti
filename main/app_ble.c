#include "app_ble.h"
#include "face_seq.h"
#include "harti_config.h"
#include "esp_log.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "ble";

/*
 * FACE_SEQ_CHAR  (UUID 0xBE01, same service as future face expression characteristic)
 * ──────────────────────────────────────────────────────────────────────────────────
 * Write payload (max 82 bytes = 2-byte header + 16 × 5-byte steps):
 *   Byte 0:  loop_count  uint8   (0=once, 0xFF=infinite)
 *   Byte 1:  step_count  uint8   (1..16)
 *   Bytes 2+N*5:
 *     [N*5+2] expr_id   uint8   (0x00..EMOTION_COUNT-1, or 0xFF=target expr)
 *     [N*5+3] hold_ms   uint16  little-endian
 *     [N*5+5] trans_ms  uint16  little-endian (0 = use expr default)
 *
 * Stop command: write [0x00, 0x00]  (step_count = 0)
 * Validation:   step_count > 16 or payload too short → reject (log error)
 *               expr_id >= EMOTION_COUNT and != 0xFF → reject
 */
void ble_handle_face_seq_write(const unsigned char *data, unsigned short len) {
    if (len < 2) return;
    uint8_t loop_count = data[0];
    uint8_t step_count = data[1];

    if (step_count == 0) {
        face_seq_stop();
        return;
    }
    if (step_count > SEQ_MAX_STEPS || (uint16_t)(2u + (uint16_t)step_count * 5u) > len) {
        ESP_LOGE(TAG, "face_seq: bad payload len=%d step_count=%d", len, step_count);
        return;
    }

    static seq_step_t steps[SEQ_MAX_STEPS];
    for (int i = 0; i < step_count; i++) {
        const uint8_t *p = data + 2 + i * 5;
        uint8_t  expr_id  = p[0];
        uint16_t hold_ms  = (uint16_t)((uint16_t)p[1] | ((uint16_t)p[2] << 8));
        uint16_t trans_ms = (uint16_t)((uint16_t)p[3] | ((uint16_t)p[4] << 8));
        if (expr_id != 0xFF && expr_id >= EMOTION_COUNT) {
            ESP_LOGE(TAG, "face_seq: invalid expr_id=%d at step %d", expr_id, i);
            return;
        }
        steps[i].expr_id  = expr_id;
        steps[i].hold_ms  = hold_ms;
        steps[i].trans_ms = trans_ms;
    }
    face_seq_play_steps(steps, step_count, loop_count);
    ESP_LOGI(TAG, "face_seq: playing %d steps loops=%d", step_count, loop_count);
}

void ble_start(void) {
    ESP_LOGI(TAG, "BLE not implemented — FACE_SEQ_CHAR stub ready (call ble_handle_face_seq_write when characteristic written)");
}
