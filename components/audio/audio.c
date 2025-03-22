#include "audio.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/projdefs.h"
#include "hal/i2s_types.h"
#include "driver/i2s.h"

#include "mp3dec.h"

#define I2S_PORT_NUM            (0)
#define SAMPLE_RATE             44100
#define PI                      (3.14159265)
#define WAVE_FREQ_HZ            (400)
#define BIT_PER_SAMPLE          16

#define AUDIO_BUFFER_SIZE       16000

struct audio_source {
    char file_path[128];
    bool is_file;
    SemaphoreHandle_t lock;
};

static const char *I2S_TAG = "I2S";
static const char *AUDIO_TAG = "Audio";

enum {
    AUD_NONE = 0,
    AUD_PLAY,
    AUD_PAUSE,
    AUD_SWAP,
    AUD_STOP,
};

static struct audio_source SOURCE = {
    .file_path = "",
    .is_file = false
};

static bool _IS_PAUSED = false;
static bool _IS_STOPPED = true;

static TaskHandle_t AUDIO_HANDLE = NULL;

void aud_main(void* unused);

/**
 * Return true if a loaded audio source should exit.
 * False if an audio source currently playing, should not exit
 */
bool _handle_messages() {
    uint32_t ret = ulTaskNotifyTake(pdTRUE, 0);
    switch (ret) {
        case AUD_NONE:
            break;
        case AUD_PLAY:
            i2s_start(I2S_PORT_NUM);
            _IS_PAUSED = false; break;
            return false;
        case AUD_PAUSE:
            i2s_stop(I2S_PORT_NUM);
            _IS_PAUSED = true; break;
            return false;
        case AUD_SWAP:
            i2s_stop(I2S_PORT_NUM);
            i2s_zero_dma_buffer(I2S_PORT_NUM);
            _IS_PAUSED = false;
            _IS_STOPPED = false;
            return true;
        case AUD_STOP:
            i2s_stop(I2S_PORT_NUM);
            i2s_zero_dma_buffer(I2S_PORT_NUM);
            _IS_PAUSED = false;
            _IS_STOPPED = true;
            return true;
    };
    return false;
}

bool _handle_controls() {
    while (1) {
        bool to_exit = _handle_messages();
        if (to_exit) {
            return true;
        }
        else if (!_IS_PAUSED) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));    
    }
    return false;
}

aud_err_t aud_init(const struct aud_i2s_config_t *config) {
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
        return AUD_FAIL;
    } else if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(I2S_TAG, "Out of Memory: Unable to install i2s driver.");
        return AUD_FAIL;
    } else {
        ESP_LOGE(I2S_TAG, "Unknown error return in i2s driver install.");
        return AUD_FAIL;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = config->bclk_gpio,
        .ws_io_num = config->lrc_gpio,
        .data_out_num = config->dout_gpio,
        .data_in_num = config->din_gpio
    };

    ESP_LOGI(I2S_TAG, "Setting Pin Configuration.");
    ret = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (ret == ESP_OK) {
        ESP_LOGI(I2S_TAG, "Successfully set i2s pin coniguration.");
        vSemaphoreCreateBinary(SOURCE.lock);
        ret = xTaskCreate(aud_main, "Audio Main", 2048, NULL, 32, &AUDIO_HANDLE);
        return AUD_OKAY;
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(I2S_TAG, "Invalid Argument in setting i2s pin configuration.");
        return AUD_FAIL;
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(I2S_TAG, "IO Error in setting i2s pin configuration.");
        return AUD_FAIL;
    } else {
        ESP_LOGE(I2S_TAG, "Unknown error return in setting i2s pin configuration.");
        return AUD_FAIL;
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


void sine_wave() {
    printf("Playing Sine Wave\n");
    short *output_buffer = malloc(8 * 4410 * sizeof(short));
    if (!output_buffer) {
        ESP_LOGE(AUDIO_TAG, "Input Buffer failed to allocate.");
        return;
    }
    size_t i2s_bytes_written = 0;
    int j = 0;
    i2s_set_clk(I2S_PORT_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    i2s_start(I2S_PORT_NUM);
    while (1) {
        if (_handle_controls()) {
            break;
        }
        for (int i = 1; i < 4 * 4410; i++) {
            output_buffer[2 * i] = (short)(sin(PI * (float) (2 * 441 * j) / (float) SAMPLE_RATE) * (float) 0x00ff);
            output_buffer[2 * i + 1] = output_buffer[2 * i];
            j++;
        }

        i2s_write(I2S_PORT_NUM, output_buffer, 8 * 4410 * sizeof(short), &i2s_bytes_written, portMAX_DELAY);
        vTaskDelay(1);
    }
    free(output_buffer);
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
    return samples_decoded;
}

void play_mp3(const void *filepath_v) {
    const char* filepath = (char*) filepath_v;
    
    HMP3Decoder mp3d = MP3InitDecoder();

    FILE *audio_file = fopen(filepath, "r");
    unsigned char *input_buffer = malloc(AUDIO_BUFFER_SIZE * sizeof(unsigned char));
    short *output_buffer = malloc(10 * 2304 * sizeof(short));

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
    i2s_start(I2S_PORT_NUM);
    while (unfinished_file) {
        if (_handle_controls()) {
            break;
        }
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
            _IS_STOPPED = true;
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
        vTaskDelay(pdMS_TO_TICKS(5));
    };
    // Clean up
    ESP_LOGI(AUDIO_TAG, "Cleaning mp3 decoder.");
    MP3FreeDecoder(mp3d);
    ESP_LOGI(AUDIO_TAG, "Cleaning input buffer.");
    free(input_buffer);
    ESP_LOGI(AUDIO_TAG, "Closing audio file.");
    fclose(audio_file);
    ESP_LOGI(AUDIO_TAG, "Cleaning output buffer.");
    free(output_buffer);
}
aud_err_t aud_play_mp3(char* filepath) {
    ESP_LOGI(AUDIO_TAG, "Notifying audio task to switch to playing mp3 file.");
    // Obtain the lock to prevent the audio loop from reading audio source before it haqs been written.
    if (xSemaphoreTake(SOURCE.lock, 0)) {
        eTaskState state = eTaskGetState(AUDIO_HANDLE);
        uint32_t ret = pdPASS;
        if (state != eSuspended) {
            ret = xTaskNotify(AUDIO_HANDLE, AUD_SWAP, eSetValueWithoutOverwrite);
        }
        else {
            vTaskResume(AUDIO_HANDLE);
        }
        if (ret == pdPASS) {
            ESP_LOGI(AUDIO_TAG, "Switching to playing mp3 file.");
            SOURCE.is_file = true;
            strcpy(SOURCE.file_path, filepath);
            xSemaphoreGive(SOURCE.lock);
            return AUD_OKAY;
        }
        else {
            ESP_LOGI(AUDIO_TAG, "Failed to send play mp3 notification. The previous message may not have been accepted");
            xSemaphoreGive(SOURCE.lock);
            return AUD_FAIL;
        }
    } else {
        ESP_LOGI(AUDIO_TAG, "Failed to obtain audio source lock.");
        return AUD_FAIL;
    }
}
aud_err_t aud_play_sine(uint32_t freq) {
    ESP_LOGI(AUDIO_TAG, "Notifying audio task to switch to playing sine wave");
    if (xSemaphoreTake(SOURCE.lock, 0)) {
        eTaskState state = eTaskGetState(AUDIO_HANDLE);
        uint32_t ret = pdPASS;
        if (state != eSuspended) {
            ret = xTaskNotify(AUDIO_HANDLE, AUD_SWAP, eSetValueWithoutOverwrite);
        }
        else {
            vTaskResume(AUDIO_HANDLE);
        }
        if (ret == pdPASS) {
            ESP_LOGI(AUDIO_TAG, "Switching to playing sine wave.");
            SOURCE.is_file = false;
            SOURCE.file_path[0] = '\0';
            xSemaphoreGive(SOURCE.lock);
            return AUD_OKAY;
        }
        else {
            ESP_LOGI(AUDIO_TAG, "Failed to send play sine notification. The previous message may not have been accepted");
            xSemaphoreGive(SOURCE.lock);
            return AUD_FAIL;
        }
    } else {
        ESP_LOGI(AUDIO_TAG, "Failed to obtain audio source lock.");
        return AUD_FAIL;
    }
}

aud_err_t aud_pause() {
    ESP_LOGI(AUDIO_TAG, "Sending pause notification.");
    uint32_t ret = xTaskNotify(AUDIO_HANDLE, AUD_PAUSE, eSetValueWithoutOverwrite);
    if (ret == pdPASS) {
        ESP_LOGI(AUDIO_TAG, "Sending pause notification successful.");
        return AUD_OKAY;
    }
    else {
        ESP_LOGI(AUDIO_TAG, "Failed to send pause notification. The previous message may not have been accepted");
        return AUD_FAIL;
    }
}


aud_err_t aud_resume() {
    ESP_LOGI(AUDIO_TAG, "Sending resume notification.");
    uint32_t ret = xTaskNotify(AUDIO_HANDLE, AUD_PLAY, eSetValueWithoutOverwrite);
    if (ret == pdPASS) {
        ESP_LOGI(AUDIO_TAG, "Sending resume notification successful.");
        return AUD_OKAY;
    }
    else {
        ESP_LOGI(AUDIO_TAG, "Failed to send resume notification. The previous message may not have been accepted");
        return AUD_FAIL;
    }
}

aud_err_t aud_stop() {
    ESP_LOGI(AUDIO_TAG, "Sending stop notification to audio task.");
    uint32_t ret = xTaskNotify(AUDIO_HANDLE, AUD_STOP, eSetValueWithoutOverwrite);
    if (ret == pdPASS) {
        ESP_LOGI(AUDIO_TAG, "Sending stop notification successful.");
        return AUD_OKAY;
    }
    else {
        ESP_LOGI(AUDIO_TAG, "Failed to send stop notification. The previous message may not have been accepted");
        return AUD_FAIL;
    }
}

void aud_main(void *unused) {

    while (1) {
        heap_caps_check_integrity(MALLOC_CAP_DEFAULT, true);
        _handle_controls();

        if (_IS_STOPPED) {
            vTaskSuspend(NULL);
        }
        const char* file_path = NULL;
        bool is_file = false;

        if (xSemaphoreTake(SOURCE.lock, pdMS_TO_TICKS(1000))) {
            file_path = SOURCE.file_path;
            is_file = SOURCE.is_file;
            xSemaphoreGive(SOURCE.lock);
        } 
        else {
             ESP_LOGW(AUDIO_TAG, "Failed to obtain audio source lock. Suspending.");
             vTaskSuspend(NULL);
        }

        if (is_file) {
            play_mp3(file_path);
        } 
        else {
        ESP_LOGI(AUDIO_TAG, "main Leftover mem: %d", (int) heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
            sine_wave();
        }
    }
}
