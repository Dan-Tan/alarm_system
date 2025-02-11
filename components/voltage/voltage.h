#ifndef _BATTERY_VOLTAGE_H
#define _BATTERY_VOLTAGE_H

#include "esp_err.h"
#include "voltage_types.h"

esp_err_t adc_config(const struct voltage_read_config_t *config);
uint32_t read_voltage(const struct voltage_read_config_t *config, uint32_t *voltage);

#endif // _BATTERY_VOLTAGE_H
