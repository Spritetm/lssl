#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "lssl_web";

#define EXTERN_FILE(name) \
	extern const char name#_start[] asm("_binary_"#name"_start"); \
	extern const char name#_end[] asm("_binary_"#name"_end");

EXTERN_FILE(lssl_js);
EXTERN_FILE(lssl_wasm);
EXTERN_FILE(lssl_wasm_map);
EXTERN_FILE(lssl_edit_css);
EXTERN_FILE(lssl_edit_html);
EXTERN_FILE(lssl_edit_js);

typedef struct {
	const char *uri;
	const char *type;
	const char *data;
	size_t len;
} web_file_t;

#define WEB_FILE(name, var) {name, var##_start, var##_end-var##_start}

const web_file_t files[]={
	WEB_FILE("/lssl.js", "application/javascript", lssl_js),
	WEB_FILE("/lssl.wasm", "application/wasm",lssl_wasm),
	WEB_FILE("/lssl.wasm.map", "application/wasm", lssl_wasm_map),
	WEB_FILE("/lssl_edit.css", "text/css", lssl_edit_css),
	WEB_FILE("/lssl_edit.html", "text/html", lssl_edit_html),
	WEB_FILE("/lssl_edit.js", "application/javascript", lssl_edit_js),
	{NULL, NULL, NULL, 0}
};

esp_err_t lssl_web_get_handler(httpd_req_t *req) {
	const web_file_t *f=files;
	while (f->name) {
		if (strcmp(req->uri, f->uri)==0) break;
		f++;
	}
	if (!f->name) {
		httpd_resp_send_404(req);
		return ESP_OK;
	}

	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, f->type);
	httpd_resp_send(req, f->data, f->len);
	return ESP_OK;
}
