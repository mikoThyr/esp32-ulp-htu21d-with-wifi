#include "esp_http_server.h"

//extern char* ssid_st;
//extern char* pass_st;
httpd_handle_t start_webserver (void);
void start_http_client ( uint8_t temperature, uint8_t humidity );
void stop_webserver (httpd_handle_t server);
