#include "audio.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/projdefs.h"
#include "hal/i2s_types.h"
#include "driver/i2s.h"

#include "mp3dec.h"

static const char *I2S_TAG = "I2S";
static const char *AUDIO_TAG = "Audio";

void init_i2s_interface() {
    esp_err_t ret;
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,  // Control and Transmit
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .dma_buf_count = 32,
        .dma_buf_len = 1024,
        .use_apll = false
    };

    ret = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(I2S_TAG, "Successfully installed i2s driver.");
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(I2S_TAG, "Invalid Argument in i2s driver install");
        return;
    } else if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(I2S_TAG, "Out of Memory: Unable to install i2s driver.");
        return;
    } else {
        ESP_LOGE(I2S_TAG, "Unknown error return in i2s driver install.");
        return;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    ESP_LOGI(I2S_TAG, "Setting Pin Configuration.");
    ret = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (ret == ESP_OK) {
        ESP_LOGI(I2S_TAG, "Successfully set i2s pin coniguration.");
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(I2S_TAG, "Invalid Argument in setting i2s pin configuration.");
        return;
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(I2S_TAG, "IO Error in setting i2s pin configuration.");
        return;
    } else {
        ESP_LOGE(I2S_TAG, "Unknown error return in setting i2s pin configuration.");
        return;
    }
};

void lower_volume(short* out, int n_samples) {
    for (int i = 0; i < n_samples; i++) {
        out[i] = out[i] / 2;
    }
}

void mono_to_stereo(short* out, int n_samples) {
    for (int i = n_samples - 1; i >= 0; i--) {
        out[2 * i]     = out[i];
        out[2 * i + 1] = out[i];
    }
}


void sine_wave(void* unused) {
    printf("Playing Sine Wave\n");
    bool paused = false;
    short *output_buffer = malloc(8 * 4410 * sizeof(short));
    size_t i2s_bytes_written = 0;
    int j = 0;
    i2s_set_clk(I2S_PORT_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    while (1) {
        if (!paused) {
            paused = pdPASS == xTaskNotifyWait(0, ULONG_MAX, NULL, 0);
            if (paused) {
                i2s_stop(I2S_PORT_NUM);
                continue;
            }
        } else {
            uint32_t ret = xTaskNotifyWait(0, ULONG_MAX, NULL, pdMS_TO_TICKS(100));
            paused = !(ret == pdPASS);
            if (!paused) {
                i2s_start(I2S_PORT_NUM);
            } else {
            continue;
            }
        }
        for (int i = 1; i < 4 * 4410; i++) {
            output_buffer[2 * i] = (short)(sin(PI * (float) (2 * 441 * j) / (float) SAMPLE_RATE) * (float) 0x00ff);
            output_buffer[2 * i + 1] = output_buffer[2 * i];
            j++;
        }

        i2s_write(I2S_PORT_NUM, output_buffer, 8 * 4410 * sizeof(short), &i2s_bytes_written, portMAX_DELAY);
        vTaskDelay(1);
    }
}

void log_mp3_err_ret(int ret, bool frame) {
        
    switch (ret) {
        case ERR_MP3_NONE:
            ESP_LOGD(AUDIO_TAG, "MP3 Decode: ESP_MP3_NONE."); return;
        case ERR_MP3_INDATA_UNDERFLOW:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INDATA_UNDERFLOW."); break;
        case ERR_MP3_MAINDATA_UNDERFLOW:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_MAINDATA_UNDERFLOW."); break;
        case ERR_MP3_FREE_BITRATE_SYNC:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_FREE_BITRATE_SYNC."); break;
        case ERR_MP3_OUT_OF_MEMORY:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_OUT_OF_MEMORY."); break;
        case ERR_MP3_NULL_POINTER:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_NULL_POINTER."); break;
        case ERR_MP3_INVALID_FRAMEHEADER:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_FRAMEHEADER."); break;
        case ERR_MP3_INVALID_SIDEINFO:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_SIDEINFO."); break;
        case ERR_MP3_INVALID_SCALEFACT:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_SCALEFACT."); break;
        case ERR_MP3_INVALID_HUFFCODES:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_HUFFCODES."); break;
        case ERR_MP3_INVALID_DEQUANTIZE:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_DEQUANTIZE."); break;
        case ERR_MP3_INVALID_IMDCT:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_IMDCT."); break;
        case ERR_MP3_INVALID_SUBBAND:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_INVALID_SUBBAND."); break;
        default:
            ESP_LOGW(AUDIO_TAG, "MP3 Decode: ERR_MP3_ERR_UNKOWN."); break;
    }
    if (frame) {
        ESP_LOGI(AUDIO_TAG, "Frame error code returned.");
    } else {
        ESP_LOGI(AUDIO_TAG, "Decoding error code returned.");
    }
    return;
}

int decode_n_frames(
        int n_frames,
        HMP3Decoder *mp3d,
        unsigned char* input_buffer,
        int *input_buffer_size,
        short *output_buffer,
        MP3FrameInfo *frame_info) {

    int samples_decoded = 0;
    unsigned char **input_buffer_ref = &input_buffer;

    int err_d = 0;
    int i = 0;
    while (i < n_frames && *input_buffer_size > 8000) {
        int offset = MP3FindSyncWord(*input_buffer_ref, *input_buffer_size);

        if (offset < 0) {
            // Force audio buffer to completely empty
            ESP_LOGI(AUDIO_TAG, "Exiting, no frame header found.");
            *input_buffer_size = 0;
            return 0;
        }
        *input_buffer_ref += offset;
        *input_buffer_size -= offset;


        err_d = MP3GetNextFrameInfo(mp3d, frame_info, *input_buffer_ref);
        log_mp3_err_ret(err_d, true);
        if (err_d < 0) {
            *input_buffer_ref += 1;
            *input_buffer_size -= 1;
            continue;
        }
        err_d = MP3Decode(mp3d,
                          input_buffer_ref,
                          input_buffer_size,
                          output_buffer + samples_decoded,
                          0);
        log_mp3_err_ret(err_d, false);

        if (err_d == ERR_MP3_INVALID_HUFFCODES) {
            break;
        }

        if (err_d < 0) {
            *input_buffer_ref += 1;
            *input_buffer_size -= 1;
            continue;
        }

        if (frame_info->nChans == 1) {
            mono_to_stereo(output_buffer + samples_decoded, frame_info->outputSamps);
            samples_decoded += frame_info->outputSamps;
        }

        samples_decoded += frame_info->outputSamps;
        ++i;
    }
    ESP_LOGI(AUDIO_TAG, "Exiting, decoded n_frames.");
    printf("Decoded: %d frames", samples_decoded);
    return samples_decoded;
}

void play_mp3(void *filepath_v) {
    const char* filepath = (char*) filepath_v;
    
    HMP3Decoder mp3d = MP3InitDecoder();

    FILE *audio_file = fopen(filepath, "r");
    unsigned char *input_buffer = malloc(AUDIO_BUFFER_SIZE * sizeof(unsigned char));
    short *output_buffer = malloc(10 * 1100 * sizeof(short));
    bool paused = false;

    printf("Input Buffer Size: %d\n", AUDIO_BUFFER_SIZE);
    printf("Output Buffer Size: %d\n", 10 * 1100);
    if (!audio_file) {
        ESP_LOGE(AUDIO_TAG, "Failed to open audio file.");
    }

    if (!input_buffer) {
        ESP_LOGE(AUDIO_TAG, "Input Buffer failed to allocate.");
        return;
    }

    if (!output_buffer) {
        ESP_LOGE(AUDIO_TAG, "Output Buffer failed to allocate.");
        return;
    }

    size_t i2s_bytes_written = 0;
    size_t bytes_to_read = AUDIO_BUFFER_SIZE;
    int input_buffer_size = 0;
    MP3FrameInfo frame_info; 

    i2s_set_clk(I2S_PORT_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    
    bool unfinished_file = true;
    while (unfinished_file) {
        if (!paused) {
            paused = pdPASS == xTaskNotifyWait(0, ULONG_MAX, NULL, 0);
            if (paused) {
                i2s_stop(I2S_PORT_NUM);
                continue;
            }
        } else {
            uint32_t ret = xTaskNotifyWait(0, ULONG_MAX, NULL, pdMS_TO_TICKS(100));
            paused = !(ret == pdPASS);
            if (!paused) {
                i2s_start(I2S_PORT_NUM);
            } else {
            continue;
            }
        }
        printf("bytes_to_read: %d, input_buffer_size: %d\n", (int)bytes_to_read, input_buffer_size);
        unsigned long bytes_read = fread(input_buffer + input_buffer_size, 
                                         sizeof(char), 
                                         bytes_to_read, 
                                         audio_file);

        if (bytes_read != bytes_to_read) {
            if (feof(audio_file)) {
                ESP_LOGI(AUDIO_TAG, "End of file encounted. Exiting or finishing stream.");
                if (bytes_read == 0) {
                    break;
                } else {
                    unfinished_file = false; 
                }
            } else { // retry read
                bytes_read = fread(input_buffer + input_buffer_size, 
                                         sizeof(char), 
                                         bytes_to_read, 
                                         audio_file);
                if (bytes_read == 0) {
                    break;
                }
            }
        }
        input_buffer_size += bytes_read;

        if (input_buffer_size == 0) {
            break;
        }

        int samples_decoded = decode_n_frames(
                10,
                mp3d, 
                input_buffer, 
                &input_buffer_size, 
                output_buffer, 
                &frame_info
                );

        if (samples_decoded != 0) { 
            lower_volume(output_buffer, samples_decoded);
        }

        bytes_to_read = AUDIO_BUFFER_SIZE - input_buffer_size;
        if (samples_decoded == 0) {
            continue;
        }

        memmove(input_buffer, input_buffer + AUDIO_BUFFER_SIZE - input_buffer_size, input_buffer_size);

        i2s_write(I2S_PORT_NUM, output_buffer, samples_decoded * sizeof(short), &i2s_bytes_written, portMAX_DELAY);
        if (samples_decoded * sizeof(short) > i2s_bytes_written) {
            //printf("Decoded: %d, Written: %u\n", (int) (samples_decoded * sizeof(short)), i2s_bytes_written);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    };
    if (audio_file == NULL) {
        ESP_LOGI(AUDIO_TAG, "Cleaning mp3 decoder.");
    }
    // Clean up
    ESP_LOGI(AUDIO_TAG, "Cleaning mp3 decoder.");
    MP3FreeDecoder(mp3d);
    ESP_LOGI(AUDIO_TAG, "Cleaning input buffer.");
    free(input_buffer);
    ESP_LOGI(AUDIO_TAG, "Closing audio file.");
    fclose(audio_file);
    ESP_LOGI(AUDIO_TAG, "Cleaning output buffer.");
    free(output_buffer);
    vTaskDelete( NULL );
}

