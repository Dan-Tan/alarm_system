#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdint.h>

// I2S pins for amp
#define I2S_LRC                 26    // Left Right Clock (A.K.A. WS) #define I2S_BCLK                25    // Bit Clock
#define I2S_BCLK                25    // Bit Clock
#define I2S_DOUT                23    // Data Out ESP32 (Data In Amp)
#define I2S_UNUSED              (-1)  // Not used

struct aud_i2s_config_t {
    uint32_t lrc_gpio;
    uint32_t bclk_gpio;
    uint32_t dout_gpio;
    uint32_t din_gpio;
};

typedef enum {
    AUD_OKAY = 0,
    AUD_FAIL = 1
} aud_err_t;

aud_err_t aud_init(const struct aud_i2s_config_t *config);

aud_err_t aud_play_sine(uint32_t freq);

aud_err_t aud_play_mp3(char* filepath);

aud_err_t aud_pause();

aud_err_t aud_resume();

aud_err_t aud_stop();

#endif
