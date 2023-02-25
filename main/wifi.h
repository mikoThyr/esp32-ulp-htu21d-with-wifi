#include <stdio.h>
#include <stdint.h>
#include "esp_wifi.h"

#define ESP_WIFI_SSID_AP	CONFIG_ESP_WIFI_SSID_AP
#define ESP_WIFI_PASS_AP	CONFIG_ESP_WIFI_PASSWORD_AP
#define ESP_WIFI_CHANNEL	CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN		CONFIG_ESP_MAX_STA_CONN

#define ESP_WIFI_SSID_ST	CONFIG_ESP_WIFI_SSID_ST
#define ESP_WIFI_PASS_ST	CONFIG_ESP_WIFI_PASSWORD_ST

uint8_t split_http_post (char* content, int length); 
void configure_nvs(void);
void configure_wifi(void);
esp_err_t start_wifi (wifi_mode_t set_mode);
