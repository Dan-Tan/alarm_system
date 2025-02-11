#include "driver/adc_common.h"
#include "hal/adc_types.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "voltage.h"
#include "voltage_types.h"

const char *VOLT_TAG = "VOLTAGE";

esp_err_t adc_config(const struct voltage_read_config_t *config) {
    esp_err_t ret;
    if (config->unit == ADC_UNIT_1) {
        ret = adc1_config_width(config->width);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = adc1_config_channel_atten((adc1_channel_t)config->channel, config->atten);
    } else {
        ret = adc2_config_channel_atten((adc2_channel_t)config->channel, config->atten);
    }
    return ret;
}

void apply_coef(const struct divider_coef_t *coef, uint32_t *out) {
    *out = *out * coef->numerator / coef->denominator;
}

uint32_t read_voltage(const struct voltage_read_config_t *config, uint32_t *voltage) {
    if (!config) {
        return ERR_VOLTAGE_NO_CONFIG;
    }

    //Continuously sample ADC1
    esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
            config->unit, 
            config->atten, 
            config->width, 
            config->default_vref, 
            adc_chars
    );

    uint32_t adc_reading = 0;
    // Multisampling
    for (int i = 0; i < config->n_samples; i++) {
        if (config->unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)config->channel);
        } else {
            int raw = 0;
            adc2_get_raw((adc2_channel_t)config->channel, config->width, &raw);
            adc_reading += raw;
        }
    }
    adc_reading /= config->n_samples;
    // Convert adc_reading to voltage in mV
    *voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    apply_coef(&config->div_coef, voltage);
    return ERR_VOLTAGE_NONE;
}
