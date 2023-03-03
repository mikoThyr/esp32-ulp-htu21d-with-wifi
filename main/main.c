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

#include "wifi.h"
#include "http.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

void rtc_initialization ( void );

void app_main(void) {
  esp_err_t error_status;

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if ( cause != ESP_SLEEP_WAKEUP_ULP ) {
	ulp_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
	} else {
    uint8_t temperature = (ulp_temp_conv & UINT16_MAX) - 40;
    uint8_t humidity = (ulp_hum_conv & UINT16_MAX);

    configure_nvs();
	  configure_wifi();
	  //configure_wifi_button();

	  start_wifi(WIFI_MODE_STA);
	  error_status = esp_wifi_start();
	  printf("start_wifi app_main(): %s\n", esp_err_to_name(error_status));

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    if ( !error_status ) {
      start_http_client(temperature, humidity);
      if ( ulp_current_measurement & UINT16_MAX ) {
        printf("Ostatni pomiar: Wilgotność\n");
      } else {
        printf("Ostatni pomiar: Temperatura\n");
      }
      printf("Temp: %d, ", temperature);
      printf("Hum: %d\n", humidity);
    }
    printf("humidity_buffer[0]: %d\n", (uint16_t)(&ulp_humidity_buffer)[0]);
    printf("humidity_buffer[1]: %d\n", (uint16_t)(&ulp_humidity_buffer)[1]);
    printf("humidity_buffer[2]: %d\n", (uint16_t)(&ulp_humidity_buffer)[2]);
    printf("humidity_buffer[3]: %d\n", (uint16_t)(&ulp_humidity_buffer)[3]);
    printf("humidity_buffer[4]: %d\n", (uint16_t)(&ulp_humidity_buffer)[4]);
    printf("humidity_buffer[5]: %d\n", (uint16_t)(&ulp_humidity_buffer)[5]);
    printf("humidity_buffer[6]: %d\n", (uint16_t)(&ulp_humidity_buffer)[6]);
    printf("humidity_buffer[7]: %d\n", (uint16_t)(&ulp_humidity_buffer)[7]);
    printf("humidity_buffer[8]: %d\n", (uint16_t)(&ulp_humidity_buffer)[8]);
    printf("humidity_buffer[9]: %d\n", (uint16_t)(&ulp_humidity_buffer)[9]);

    printf("temperature_buffer[0]: %d\n", (uint16_t)(&ulp_temperature_buffer)[0]);
    printf("temperature_buffer[1]: %d\n", (uint16_t)(&ulp_temperature_buffer)[1]);
    printf("temperature_buffer[2]: %d\n", (uint16_t)(&ulp_temperature_buffer)[2]);
    printf("temperature_buffer[3]: %d\n", (uint16_t)(&ulp_temperature_buffer)[3]);
    printf("temperature_buffer[4]: %d\n", (uint16_t)(&ulp_temperature_buffer)[4]);
    printf("temperature_buffer[5]: %d\n", (uint16_t)(&ulp_temperature_buffer)[5]);
    printf("temperature_buffer[6]: %d\n", (uint16_t)(&ulp_temperature_buffer)[6]);
    printf("temperature_buffer[7]: %d\n", (uint16_t)(&ulp_temperature_buffer)[7]);
    printf("temperature_buffer[8]: %d\n", (uint16_t)(&ulp_temperature_buffer)[8]);
    printf("temperature_buffer[9]: %d\n", (uint16_t)(&ulp_temperature_buffer)[9]);

    esp_wifi_disconnect();
    esp_wifi_stop();
  }

  rtc_initialization();

  ulp_run(&ulp_entry - RTC_SLOW_MEM);

  esp_sleep_enable_ulp_wakeup();
  esp_deep_sleep_start();
}


void rtc_initialization ( void ) {
  rtc_gpio_init(GPIO_NUM_25);
	rtc_gpio_pullup_en(GPIO_NUM_25);
	rtc_gpio_set_direction(GPIO_NUM_25, RTC_GPIO_MODE_INPUT_OUTPUT_OD);

  rtc_gpio_init(GPIO_NUM_26);
	rtc_gpio_pullup_en(GPIO_NUM_26);
	rtc_gpio_set_direction(GPIO_NUM_26, RTC_GPIO_MODE_INPUT_OUTPUT_OD);
}
