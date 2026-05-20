#ifndef HARTI_TEMP_H
#define HARTI_TEMP_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t temp_init(void);
float temp_read_celsius(void);

#ifdef __cplusplus
}
#endif

#endif
