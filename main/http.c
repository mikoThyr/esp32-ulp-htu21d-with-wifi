#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_tls_crypto.h"

#include "http.h"
#include "wifi.h"

static const char *TAG = "HTTP";

esp_err_t get_handler (httpd_req_t *req) {
	const char resp[] =
		"<html>"
		"<head>"
			"<title>ESP32</title>"
			"<style type=\"text/css\">"
				"body { background-color: #6591A8; }"
			"</style>"
		"</head>"
		"<body>"
			"<div class=\"outer\"><br><br>"
			"<form action=\"/url\" method=\"POST\">"
				"<h3>Complete the fields below with the network ssid and password</h3><br><br>"
				"<label for=\"fname\">SSID:</label><br>"
				"<input type=\"text\" id=\"ssid\" name=\"ssid\"><br><br>"
				"<label for=\"lname\">Password:</label><br>"
				"<input type=\"text\" id=\"pass\" name=\"pass\"><br>"
				"<input type=\"submit\" value=\"Save\">"
			"</form>"
			"</div>"
		"</body>"
		"</html>";

	ESP_ERROR_CHECK(httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN));
	return ESP_OK;
}

esp_err_t post_handler (httpd_req_t *req) {
	char content[52];
	/* Truncate if content length larger than the buffer */
	size_t recv_size = MIN(req->content_len, sizeof(content));
	int ret = httpd_req_recv(req, content, recv_size);

	split_http_post(content, ret);

	if (ret <= 0) {
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			httpd_resp_send_408(req);
		}
		return ESP_FAIL;
	}

	const char resp[] =
		"<html>"
		"<head>"
			"<title>ESP32</title>"
			"<style type=\"text/css\">"
				"body { background-color: #6591A8; }"
			"</style>"
		"</head>"
		"<body>"
			"<h3>OK.</h3>"
		"</body>"
		"</html>";

	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

httpd_uri_t uri_get = {
	.uri = "/uri",
	.method = HTTP_GET,
	.handler = get_handler,
	.user_ctx = NULL
};

httpd_uri_t uri_post = {
	.uri       = "/url",
	.method    = HTTP_POST,
	.handler   = post_handler,
	.user_ctx  = NULL
};

httpd_handle_t start_webserver (void) {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;
	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_register_uri_handler(server, &uri_get);
		httpd_register_uri_handler(server, &uri_post);
	}
	return server;
}

void stop_webserver (httpd_handle_t server) {
	if (server) {
		httpd_stop(server);
	}
}

void start_http_client ( uint8_t temperature, uint8_t humidity ) {
    char post_data[50];
    snprintf(post_data, sizeof(post_data), "temperature=%d&humidity=%d", temperature, humidity);

    esp_http_client_config_t config = {
        //.url = "http://192.168.1.10:5000/",
        .host = "192.168.1.10",
        .path = "/",
        .port = 5000,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 1000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            ESP_LOGI(TAG, "POST request successful");
        } else {
            ESP_LOGI(TAG, "POST request failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "Error performing POST request: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    esp_http_client_close(client);

}

