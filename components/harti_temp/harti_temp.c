#include "harti_temp.h"
#include <math.h>
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "harti_temp";

#define NTC_BETA    3950.0f
#define NTC_R_REF   10000.0f
#define NTC_R_25C   10000.0f
#define NTC_T_25C   298.15f
#define ADC_CHANNEL ADC_CHANNEL_0
#define ADC_UNIT    ADC_UNIT_1
#define V_SUPPLY_MV 3300.0f

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool cali_enabled;

esp_err_t temp_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_cfg, &adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(err));
        return err;
    }

    cali_enabled = false;
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "ADC calibration scheme not available (eFuse not burned), using raw conversion");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC calibration: %s", esp_err_to_name(err));
    } else {
        cali_enabled = true;
        ESP_LOGI(TAG, "ADC calibration enabled");
    }

    ESP_LOGI(TAG, "harti_temp initialized");
    return ESP_OK;
}

float temp_read_celsius(void)
{
    int raw;
    esp_err_t err = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return 25.0f;
    }

    float v_mv;
    if (cali_enabled) {
        int v_out;
        adc_cali_raw_to_voltage(cali_handle, raw, &v_out);
        v_mv = (float)v_out;
    } else {
        v_mv = (float)raw * V_SUPPLY_MV / 4095.0f;
    }

    float r_ntc = NTC_R_REF * (V_SUPPLY_MV - v_mv) / v_mv;

    float temp_k = 1.0f / (
        1.0f / NTC_T_25C +
        (1.0f / NTC_BETA) * logf(r_ntc / NTC_R_25C)
    );

    return temp_k - 273.15f;
}

int temp_read_raw(void)
{
    int raw;
    esp_err_t err = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
    if (err != ESP_OK) {
        return 4095; /* 读取失败返回最大值，避免误判按键按下 */
    }
    return raw;
}
