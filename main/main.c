#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"

#include "esp32/ulp.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "hal/gpio_types.h"
#include "hal/rtc_io_types.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "ulp_common.h"
#include "ulp_main.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_log.h"

#include "driver/i2c.h"

#include "wifi.h"
#include "http.h"

#include "nvs.h"
#include "nvs_flash.h"

#define LED_PIN			  GPIO_NUM_14
#define GPIO_SDA		  GPIO_NUM_25
#define GPIO_SCL		  GPIO_NUM_26
#define I2C_ADR			  0x80
#define I2C_REG_WRITE	0xE6
#define I2C_REG_READ	0xE7
#define RESET         0xFE

#define I2C_RESOLUTION	3		//			RH[bits]	Temp[bits]
#if I2C_RESOLUTION == 0			//0. 00		12			14
#define RESOLUTION 			0x00
#elif I2C_RESOLUTION == 1		//1. 01		8			12
#define RESOLUTION 			0x01
#elif I2C_RESOLUTION == 2		//2. 10		10			13
#define RESOLUTION 			0x80
#elif I2C_RESOLUTION == 3		//3. 11		11			11
#define RESOLUTION			0x81
#endif

enum {
	NO_ACK = 0,
	ACK
};

i2c_config_t GPIO_I2C_Init = {
	.mode = I2C_MODE_MASTER,
	.sda_io_num = GPIO_SDA,
	.sda_pullup_en = GPIO_PULLUP_ENABLE,
	.scl_io_num = GPIO_SCL,
	.scl_pullup_en = GPIO_PULLUP_ENABLE,
	.master.clk_speed = 100000
};

gpio_config_t io_conf = {
	.intr_type = GPIO_INTR_DISABLE,
	.mode = GPIO_MODE_OUTPUT,
	.pin_bit_mask = (1ULL << GPIO_NUM_14),
	.pull_down_en = GPIO_PULLDOWN_ENABLE,
	.pull_up_en = GPIO_PULLUP_DISABLE,
};

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

void rtc_initialization ( void );
void htu21d_set_register ( void );
void htu21d_reset ( void );

//INFO: SUPERLOOP
void app_main(void) {
  esp_err_t error_status;
  i2c_param_config(I2C_NUM_0, &GPIO_I2C_Init);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  gpio_config(&io_conf);
  configure_nvs();

  wifi_mode_t current_mode;
  nvs_handle_t nvs_handle;
  char ssid[32];
  size_t ssid_len = sizeof(ssid);
  char pass[32];
  size_t pass_len = sizeof(pass);

  error_status = nvs_open("storage", NVS_READONLY, &nvs_handle);

  error_status = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);

  error_status = nvs_get_str(nvs_handle, "pass", pass, &pass_len);

  nvs_close(nvs_handle);

  switch ( ulp_wake_sw & UINT16_MAX ) {
  //INFO: WEBSERVER
  case 1:
	  httpd_handle_t server_handle;
    printf("Wake up by pressed button.\n");
    configure_wifi();

		error_status = start_wifi(WIFI_MODE_AP);
		if (error_status == ESP_OK) {
			gpio_set_level(LED_PIN, 1);

	    server_handle = start_webserver();
			vTaskDelay(90000 / portTICK_PERIOD_MS);		//Wait x sec and turn off wifi and led
			stop_webserver(server_handle);

			gpio_set_level(LED_PIN, 0);
		}
		printf("start_wifi(): %s\n", esp_err_to_name(error_status));
    esp_wifi_stop();
    break;
  default:
    //INFO: Send data by the HTTP
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
      ulp_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
      htu21d_set_register();
      //htu21d_reset();
    } else {
      uint8_t temperature = (ulp_temp_conv & INT16_MAX) - 4;
      uint8_t humidity = (ulp_hum_conv & UINT16_MAX) + 26;

      configure_wifi();

      start_wifi(WIFI_MODE_STA);
	    error_status = esp_wifi_start();
	    printf("start_wifi app_main(): %s\n", esp_err_to_name(error_status));
      vTaskDelay(5000 / portTICK_PERIOD_MS);

      if ( !error_status ) {
        start_http_client(temperature, humidity);
      }

      printf("Temp: %d, ", temperature);
      printf("Hum: %d\n", humidity);
      esp_wifi_disconnect();
      esp_wifi_stop();
    }
    break;
  }
  rtc_initialization();
  ulp_run(&ulp_entry - RTC_SLOW_MEM);
  esp_sleep_enable_ulp_wakeup();
  esp_deep_sleep_start();
}

void rtc_initialization ( void ) {
  //INFO: HTU21D SDA line configuration.
  rtc_gpio_init(GPIO_SDA);
	rtc_gpio_pullup_en(GPIO_SDA);
	rtc_gpio_set_direction(GPIO_SDA, RTC_GPIO_MODE_INPUT_OUTPUT_OD);
  //INFO: HTU21D SCL line configuration.
  rtc_gpio_init(GPIO_SCL);
	rtc_gpio_pullup_en(GPIO_SCL);
	rtc_gpio_set_direction(GPIO_SCL, RTC_GPIO_MODE_INPUT_OUTPUT_OD);
  //INFO: Wakeup from deep sleep
  rtc_gpio_init(GPIO_NUM_27);
  rtc_gpio_pulldown_en(GPIO_NUM_27);
  rtc_gpio_set_direction(GPIO_NUM_27, RTC_GPIO_MODE_INPUT_ONLY);
}

void htu21d_reset ( void ) {
	i2c_cmd_handle_t link = i2c_cmd_link_create();
	i2c_master_start(link);
	i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_WRITE, ACK);
	i2c_master_write_byte(link, RESET, ACK);
	i2c_master_stop(link);
	i2c_master_cmd_begin(I2C_NUM_0, link, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(link);
  vTaskDelay( 20 / portTICK_PERIOD_MS );
}

void htu21d_set_register (void ) {
	uint8_t register_data;
  uint8_t rem_register_data;
	i2c_cmd_handle_t link = i2c_cmd_link_create();
	i2c_master_start(link);
	i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_WRITE, ACK);
	i2c_master_write_byte(link, I2C_REG_READ, ACK);
	i2c_master_start(link);
	i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_READ, ACK);
	i2c_master_read_byte(link, &(register_data), NO_ACK);
  rem_register_data = register_data;
	if ((register_data & 0x81) != RESOLUTION) {
		register_data = register_data & 0x7E;
		register_data = register_data | RESOLUTION;
		i2c_master_start(link);
		i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_WRITE, ACK);
		i2c_master_write_byte(link, I2C_REG_WRITE, ACK);
		i2c_master_write_byte(link, register_data, ACK);
	}
	i2c_master_stop(link);
	i2c_master_cmd_begin(I2C_NUM_0, link, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(link);
  printf("rem_register_data: %d\nRESOLUTION: %d\nrem_register_data & 0x81: %d\n", rem_register_data, RESOLUTION, rem_register_data & 0x81);
}

