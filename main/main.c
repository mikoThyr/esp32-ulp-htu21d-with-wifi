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
/**
 * @brief The function to initialize pins: switch.
 */
void gpio_initialization ( void );
/**
 * @brief The function to initialize pins: I2C (SDA, SCL).
 */
void rtc_initialization ( void );
/**
 * @brief The function to set resolution of the sensor.
 * The first step of the function is to check two bits fetched from a sensor register.
 * If that bits are the same as set by us the function just end without changing the register.
 */
void htu21d_set_register ( void );
/**
 * @brief The function used to reset a sesnor.
 */
void htu21d_reset ( void );

void app_main ( void ) {
  esp_err_t error_status;

  gpio_initialization();
  rtc_initialization();
  configure_nvs();

  switch ( ulp_wake_sw & UINT16_MAX ) {
  case 1:
	  httpd_handle_t server_handle;
    printf("Wake up by pressed button.\n");
    configure_wifi();

		error_status = start_wifi(WIFI_MODE_AP);
		if (error_status == ESP_OK) {
			gpio_set_level(LED_PIN, 1);

	    server_handle = start_webserver();
			vTaskDelay(60000 / portTICK_PERIOD_MS);		//Wait x sec and turn off wifi and led
			stop_webserver(server_handle);

			gpio_set_level(LED_PIN, 0);
		}
		printf("start_wifi(): %s\n", esp_err_to_name(error_status));
    esp_wifi_stop();
    break;
  default:
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

void gpio_initialization ( void ) {
  i2c_param_config(I2C_NUM_0, &GPIO_I2C_Init);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  gpio_config(&io_conf);
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
	i2c_cmd_handle_t link = i2c_cmd_link_create();
	i2c_master_start(link);
	i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_WRITE, ACK);
	i2c_master_write_byte(link, I2C_REG_READ, ACK);
	i2c_master_start(link);
	i2c_master_write_byte(link, I2C_ADR | I2C_MASTER_READ, ACK);
	i2c_master_read_byte(link, &(register_data), NO_ACK);
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
}

