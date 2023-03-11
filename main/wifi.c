#include <stdio.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include <esp_system.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi.h"

typedef struct {
  char ssid[32];
  char pass[32];
} user_data_t;

static esp_netif_t *netif_ap = NULL;
static esp_netif_t *netif_sta = NULL;

user_data_t read_nvm_data ( void ) {
  nvs_handle_t nvs_handle;
  user_data_t value = {0};
  esp_err_t err;

  size_t ssid_len = sizeof(value.ssid);
  size_t pass_len = sizeof(value.pass);

  err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  ESP_ERROR_CHECK(err);
  err = nvs_get_str(nvs_handle, "ssid", value.ssid, &ssid_len);
  ESP_ERROR_CHECK(err);
  err = nvs_get_str(nvs_handle, "pass", value.pass, &pass_len);
  ESP_ERROR_CHECK(err);
  nvs_close(nvs_handle);

  return value;
}

void configure_nvs (void) {
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

void wifi_sta_mode ( void ) {
  esp_err_t err;
  user_data_t value = read_nvm_data ();

  wifi_config_t wifi_config = {
 	  //.sta = {
 			//.ssid = ssid,
 			//.password = pass,
 		//},
  };

  strcpy((char* )wifi_config.sta.ssid, value.ssid);
  strcpy((char* )wifi_config.sta.password, value.pass);

  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	err = esp_wifi_connect();
	printf("wifi_sta_mode(): %s\n", esp_err_to_name(err));
}

static void event_handler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		esp_wifi_connect();
		printf("Retry to connect to the AP\n");
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

	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );

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
			if (set_mode & WIFI_MODE_STA) {
				//error_status = esp_wifi_start();
			} else {
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
				//error_status = esp_wifi_start();
			} else {
				//ESP_ERROR_CHECK(esp_wifi_stop());
				//vTaskDelay(10 / portTICK_PERIOD_MS);
				//error_status = esp_wifi_start();
				wifi_ap_mode();
			}
			break;
		case WIFI_MODE_NULL:
			if (set_mode & WIFI_MODE_STA) {
				error_status = esp_wifi_start();
				wifi_sta_mode();
			} else if (set_mode & WIFI_MODE_AP) {
				error_status = esp_wifi_start();
				wifi_ap_mode();
			} else {
				error_status = ESP_FAIL;
			}
			break;
		default:
			break;
		}
	}
	return error_status;
}
