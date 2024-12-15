#include <stdio.h>
#include "esp_wifi.h"
#include "lssl_idf_web.h"
#include "wifi_manager.h"
#include "http_app.h"
#include "leds.h"
#include "led_syscalls.h"

#define EXTERN_FILE(name) \
	extern const char name##_start[] asm("_binary_"#name"_start"); \
	extern const long name##_length asm(#name"_length")

EXTERN_FILE(root_html);
EXTERN_FILE(root_css);
EXTERN_FILE(favicon_ico);

static esp_err_t mapper_get(httpd_req_t *req) {
	char *q=strchr(req->uri, '?');
	if (!q) {
		httpd_resp_send(req, "?", 1);
		return ESP_OK;
	}
	q++; //skip '?'
	esp_err_t r;
	char buf[128];
	r=httpd_query_key_value(q, "get_size", buf, 128);
	if (r!=ESP_ERR_NOT_FOUND) {
		sprintf(buf, "%d", leds_get_count());
		httpd_resp_send(req, buf, strlen(buf));
		return ESP_OK;
	}
	r=httpd_query_key_value(q, "set_led", buf, 128);
	if (r!=ESP_ERR_NOT_FOUND) {
		int led=atoi(buf);
		leds_set_mapper_led(led);
		httpd_resp_send(req, buf, strlen(buf));
		return ESP_OK;
	}
	httpd_resp_send(req, "?", 1);
	return ESP_OK;
}


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
	if (strcmp(req->uri, "/favicon.ico")==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "image/vnd.microsoft.icon");
		httpd_resp_send(req, favicon_ico_start, favicon_ico_length);
		return ESP_OK;
	}
	if (strncmp(req->uri, "/lssl_run/", 10)==0) {
		const char *pgm=req->uri+10;
		leds_change_program(pgm);
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/plain");
		httpd_resp_send(req, "OK", 2);
		return ESP_OK;
	}
	if (strncmp(req->uri, "/mapper", 7)==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/plain");
		return mapper_get(req);
	}
	printf("No GET handler found for '%s'!\n", req->uri);
	httpd_resp_set_status(req, "404 Not Found");
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_send(req, "Not Found", 9);
	return ESP_OK;
}

static void prog_changed(const char *name) {
	leds_change_program(name);
}

void app_main() {
	wifi_manager_start();
	http_app_set_handler_hook(HTTP_GET, &lssl_web_get_handler);
	http_app_set_handler_hook(HTTP_POST, &lssl_web_post_handler);
	lssl_http_app_set_handler_hook(HTTP_GET, &get_handler);
	leds_init(8);
	lssl_web_register_program_changed_cb(prog_changed);
	lssl_web_init(leds_get_count());
}
