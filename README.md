## Description
---

Program for collecting temperature measurements from the sensor (**HTU21D** [^1]) via **ESP32-WROOM-32D** [^2]. Measurements are made in the deep sleep mode where they are also checked in several steps that eliminate occasional glitches and limit values. New values are sent via wifi to an external server via HTTPD. The user can use the button to switch the device into the server where after connecting, he can set password and ssid for the network.


To communicate with sensor were use two functions from [**esp32-ulp-i2c**](https://github.com/mikoThyr/esp32-ulp-i2c) project.



[^1]:[HTU21D documentation](https://cdn-shop.adafruit.com/datasheets/1899_HTU21D.pdf)

[^2]:[ESP32-WROOM-32D documentation](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf)
