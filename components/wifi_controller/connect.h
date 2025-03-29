#ifndef _CONNECT_H_
#define _CONNECT_H_

#include "esp_err.h"

esp_err_t example_connect(const char* ssid, const char* password);
esp_err_t example_disconnect();

#endif
