#include <string.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi.h"

static esp_netif_t *netif_ap = NULL;
static esp_netif_t *netif_sta = NULL;

struct {
	char* data;
} user[2];

char* ssid_st;
char* pass_st;

uint8_t split_http_post (char* content, int length) {
	uint8_t number_words = 0;
	char ampersand_sign = '&';
	char equal_sign = '=';
	content[length] = ampersand_sign;

	uint8_t current_position = 0;
	uint8_t id = 0;
	uint8_t position = 0;

	/*	The characters in the "content" string looks like ssid=user_text&pass=user_text. The idea behind the loop is to separate the text after the "=" character until the ampersand character will be reached. The string is stored in the first structure element. Steps are repeated on the "pass".
	 */
	while (current_position <= length) {
		if (content[current_position] == equal_sign) {
			number_words++;
			current_position++;
			while ((content[current_position] != ampersand_sign) && (current_position <= length)) {
				user[id].data[position] = content[current_position];
				position++;
				current_position++;
			}

			printf("%s\n", user[id].data);

			switch (id) {
				case 0:
					strcpy(ssid_st, user[0].data);
					break;
				case 1:
					strcpy(pass_st, user[1].data);
					break;
				default:
					break;
			}
			position = 0;
			id++;
		}
		current_position++;
	}

	return number_words;
}

void configure_nvs (void) {
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
}

void wifi_ap_mode (void) {
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = ESP_WIFI_SSID_AP,
			.ssid_len = strlen(ESP_WIFI_SSID_AP),
			.channel = ESP_WIFI_CHANNEL,
			.password = ESP_WIFI_PASS_AP,
			.max_connection = MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
			.pmf_cfg = {
				.required = false,
			},
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
}

void wifi_sta_mode (void) {
	esp_err_t error_status;
	//char* error_code;
	wifi_config_t wifi_config = {
// 		.sta = {
// 			.ssid = ESP_WIFI_SSID_ST,
// 			.password = ESP_WIFI_PASS_ST
// 		},
	};

	strcpy((char* )wifi_config.sta.ssid, "SiecImperialna1");//ssid_st);
	strcpy((char* )wifi_config.sta.password, "1mPer1UMm");//pass_st);

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	error_status = esp_wifi_connect();
	printf("wifi_sta_mode(): %s\n", esp_err_to_name(error_status));
}

static void event_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		esp_wifi_connect();
		printf("Retry to connect to the AP\n");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

void configure_wifi (void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	netif_ap = esp_netif_create_default_wifi_ap();
	assert(netif_ap);
	netif_sta = esp_netif_create_default_wifi_sta();
	assert(netif_sta);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );

	esp_event_handler_instance_t instance_id;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_id));
}

esp_err_t start_wifi (wifi_mode_t set_mode) {
	esp_err_t error_status;
	wifi_mode_t current_mode;
	error_status = esp_wifi_get_mode(&current_mode);

	if (error_status == ESP_OK) {
		switch(current_mode) {
			case WIFI_MODE_STA:
				if (set_mode & WIFI_MODE_STA) {					//WIFI_MODE_STA
					printf("1\n");
					//error_status = esp_wifi_start();
				} else {										//WIFI_MODE_AP
					printf("2\n");
					//ESP_ERROR_CHECK(esp_wifi_stop());
					//vTaskDelay(10 / portTICK_PERIOD_MS);
					//error_status = esp_wifi_start();
					esp_wifi_disconnect();
					vTaskDelay(100 / portTICK_PERIOD_MS);
					wifi_ap_mode();
				}
				break;
			case WIFI_MODE_AP:
				if (set_mode & WIFI_MODE_AP) {
					printf("3\n");
					//error_status = esp_wifi_start();
				} else {
					printf("4\n");
					//ESP_ERROR_CHECK(esp_wifi_stop());
					//vTaskDelay(10 / portTICK_PERIOD_MS);
					//error_status = esp_wifi_start();
					wifi_ap_mode();
				}
				break;
			case WIFI_MODE_NULL:
				if (set_mode & WIFI_MODE_STA) {
					printf("5\n");
					error_status = esp_wifi_start();
					wifi_sta_mode();
				} else if (set_mode & WIFI_MODE_AP) {
					printf("6\n");
					//error_status = esp_wifi_start();
					wifi_ap_mode();
				} else {
					printf("7\n");
					error_status = ESP_FAIL;
				}
				break;
			default:
				break;
		}
	}
	return error_status;
}
