#ifndef _AUDIO_H
#define _AUDIO_H

// I2S pins for amp
#define I2S_LRC                 26    // Left Right Clock (A.K.A. WS)
#define I2S_BCLK                25    // Bit Clock
#define I2S_DOUT                33    // Data Out ESP32 (Data In Amp)
#define I2S_UNUSED              (-1)  // Not used

#define I2S_PORT_NUM            (0)
#define SAMPLE_RATE             44100
#define PI                      (3.14159265)
#define WAVE_FREQ_HZ            (400)
#define BIT_PER_SAMPLE          16

#define AUDIO_BUFFER_SIZE       16000

void init_i2s_interface();

void sine_wave(void* _unused_);

void play_mp3(void* filepath);

#endif
