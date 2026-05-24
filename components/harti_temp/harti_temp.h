#ifndef HARTI_TEMP_H
#define HARTI_TEMP_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t temp_init(void);
float temp_read_celsius(void);
int temp_read_raw(void);  /* 返回原始 ADC 值，可用于检测 BOOT 按键按下 */

#ifdef __cplusplus
}
#endif

#endif
