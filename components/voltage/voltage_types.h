#ifndef _VOLTAGE_TYPES_H
#define _VOLTAGE_TYPES_H

#include "hal/adc_types.h"

struct divider_coef_t {
    uint32_t numerator;
    uint32_t denominator;
};

struct voltage_read_config_t {
    adc_channel_t channel;
    adc_bits_width_t width;
    adc_atten_t atten;
    adc_unit_t unit;
    struct divider_coef_t div_coef;
    uint32_t default_vref;
    uint32_t n_samples;
};

enum {
    ERR_VOLTAGE_NONE = 0,
    ERR_VOLTAGE_NO_CONFIG = -1,
};

#endif // _VOLTAGE_TYPES_H
