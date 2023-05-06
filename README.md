## Description

*Program for collecting temperature measurements from the sensor (HTU21D) in deep sleep mode. New values are sent via wifi to an external server via HTTPD.*
*Masurements are checked in several steps that eliminate occasional glitches and limit values.*

*To communicate with sensor were use two functions from [**esp32-ulp-i2c**](https://github.com/mikoThyr/esp32-ulp-i2c) project.*
