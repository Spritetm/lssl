#include <stdio.h>
#include "esp_wifi.h"
#include "lssl_idf_web.h"
#include "wifi_manager.h"
#include "http_app.h"

#define EXTERN_FILE(name) \
	extern const char name##_start[] asm("_binary_"#name"_start"); \
	extern const long name##_length asm(#name"_length")

EXTERN_FILE(root_html);
EXTERN_FILE(root_css);

static esp_err_t get_handler(httpd_req_t *req) {
	if (strcmp(req->uri, "/root.html")==0 || strcmp(req->uri, "/")==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, root_html_start, root_html_length);
		return ESP_OK;
	}
	if (strcmp(req->uri, "/root.css")==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/css");
		httpd_resp_send(req, root_html_start, root_html_length);
		return ESP_OK;
	}
	httpd_resp_set_status(req, "404 Not Found");
	return ESP_OK;
}


void app_main() {
	wifi_manager_start();
	http_app_set_handler_hook(HTTP_GET, &lssl_web_get_handler);
	http_app_set_handler_hook(HTTP_POST, &lssl_web_post_handler);
	lssl_http_app_set_handler_hook(HTTP_GET, &get_handler);
}
