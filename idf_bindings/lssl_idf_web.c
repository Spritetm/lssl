#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define NVS_NAMESPACE "lssl_progs"

#define TAG "lssl_web"

#define EXTERN_FILE(name) \
	extern const char name##_start[] asm("_binary_"#name"_start"); \
	extern const long name##_length asm(#name"_length")

EXTERN_FILE(lssl_js);
EXTERN_FILE(lssl_wasm);
EXTERN_FILE(lssl_wasm_map);
EXTERN_FILE(lssl_edit_css);
EXTERN_FILE(lssl_edit_html);
EXTERN_FILE(lssl_edit_js);

typedef esp_err_t (*web_cb_t)(httpd_req_t *req);

static esp_err_t (*lssl_custom_get_httpd_uri_handler)(httpd_req_t *r) = NULL;
static esp_err_t (*lssl_custom_post_httpd_uri_handler)(httpd_req_t *r) = NULL;

static esp_err_t list_programs(httpd_req_t *req) {
	nvs_handle nvs;
	ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs));
	struct cJSON *progs=cJSON_CreateArray();
	nvs_iterator_t it=NULL;
	esp_err_t err=nvs_entry_find_in_handle(nvs, NVS_TYPE_BLOB, &it);
	while (err==ESP_OK) {
		nvs_entry_info_t info;
		nvs_entry_info(it, &info);
		if (info.key[0]=='p') {
			struct cJSON *prog=cJSON_CreateString(info.key+1);
			cJSON_AddItemToArray(progs, prog);
		}
		err=nvs_entry_next(&it);
	}
	nvs_release_iterator(it);
	nvs_close(nvs);
	char *out=cJSON_Print(progs);
	cJSON_Delete(progs);
	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, "text/json");
	httpd_resp_send(req, out, strlen(out));
	free(out);
	return ESP_OK;
}

//returns the bit after the last '/' in a string, or the entire string if no '/' found
static const char *file_in_uri(const char *in) {
	char *c=strrchr(in, '/');
	if (!c) return in;
	return c+1;
}

static esp_err_t get_program(httpd_req_t *req) {
	char name[17]={0};
	snprintf(name, 16, "p%s", file_in_uri(req->uri));

	nvs_handle nvs;
	ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs));
	size_t req_sz=0;
	char *prog;
	if (nvs_get_blob(nvs, name, NULL, &req_sz)==ESP_OK) {
		prog=calloc(req_sz+1, 1);
		nvs_get_blob(nvs, name, prog, &req_sz);
	} else {
		prog=strdup("//New file\n\n");
	}
	nvs_close(nvs);
	
	httpd_resp_set_status(req, "200 OK");
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_send(req, prog, strlen(prog));
	free(prog);
	return ESP_OK;
}

#define MAX_FILE_SIZE 32768

static esp_err_t post_save_to_nvs(httpd_req_t *req, const char *name) {
	int total_len=req->content_len;
	if (total_len>MAX_FILE_SIZE) {
		httpd_resp_set_status(req, "507 Insufficient storage");
		return ESP_OK;
	}
	char *in=calloc(total_len, 1);
	int len=0;
	while (len<total_len) {
		int recv=httpd_req_recv(req, in+len, total_len-len);
		if (recv<=0) {
			httpd_resp_set_status(req, "500 Internal error");
			return ESP_OK;
		}
		len+=recv;
	}
	
	nvs_handle nvs;
	ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
	nvs_set_blob(nvs, name, in, strlen(in));
	nvs_close(nvs);

	nvs_close(nvs);
	httpd_resp_set_status(req, "204 No content");
	return ESP_OK;
}

static esp_err_t save_program(httpd_req_t *req) {
	char name[17]={0};
	snprintf(name, 16, "p%s", file_in_uri(req->uri));
	return post_save_to_nvs(req, name);
}

static esp_err_t save_bytecode(httpd_req_t *req) {
	char name[17]={0};
	snprintf(name, 16, "b%s", file_in_uri(req->uri));
	return post_save_to_nvs(req, name);
}

typedef struct {
	const char *uri;
	const char *type;
	const char *data;
	const long *len;
	web_cb_t cb;
} web_file_t;

#define WEB_FILE(name, type, var) {name, type, var##_start, &var##_length, NULL}
#define WEB_CB(name, cb) {name, NULL, NULL, NULL, cb}

const web_file_t get_files[]={
	WEB_FILE("/lssl.js", "application/javascript", lssl_js),
	WEB_FILE("/lssl.wasm", "application/wasm",lssl_wasm),
	WEB_FILE("/lssl.wasm.map", "application/wasm", lssl_wasm_map),
	WEB_FILE("/lssl_edit.css", "text/css", lssl_edit_css),
	WEB_FILE("/lssl_edit.html", "text/html", lssl_edit_html),
	WEB_FILE("/lssl_edit.js", "application/javascript", lssl_edit_js),
	WEB_CB("/lssl_list", list_programs),
	WEB_CB("/lssl_program/*", get_program),
	{NULL, NULL, NULL, NULL, NULL}
};

const web_file_t post_files[]={
	WEB_CB("/lssl_save_program/*", save_program),
	WEB_CB("/lssl_save_compiled/*", save_bytecode),
	{NULL, NULL, NULL, NULL, NULL}
};


const web_file_t *find_web_file(const web_file_t *files, const char *uri) {
	const web_file_t *f=files;
	while (f->uri) {
		char *ao=strchr(uri, '*');
		if (ao!=NULL) {
			int len=ao-uri;
			if (strncmp(uri, f->uri, len)==0) break;
		}
		if (strcmp(uri, f->uri)==0) break;
		f++;
	}
	return f->uri?f:NULL;
}

esp_err_t lssl_web_get_handler(httpd_req_t *req) {
	const web_file_t *f=find_web_file(get_files, req->uri);
	if (f && f->cb) {
		return f->cb(req);
	}
	if (f && f->uri) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, f->type);
		httpd_resp_send(req, f->data, *f->len);
		return ESP_OK;
	}
	//Nothing found.
	if (lssl_custom_get_httpd_uri_handler) {
		return lssl_custom_get_httpd_uri_handler(req);
	} else {
		httpd_resp_send_404(req);
		return ESP_OK;
	}
}

esp_err_t lssl_web_post_handler(httpd_req_t *req) {
	const web_file_t *f=find_web_file(post_files, req->uri);
	if (f && f->cb) {
		return f->cb(req);
	}
	//Note: we don't check for 'raw' files as POSTing to them isn't really useful.
	//Nothing found.
	if (lssl_custom_post_httpd_uri_handler) {
		return lssl_custom_post_httpd_uri_handler(req);
	} else {
		httpd_resp_send_404(req);
		return ESP_OK;
	}
}

esp_err_t lssl_http_app_set_handler_hook(httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r)){
	if(method == HTTP_GET) {
		lssl_custom_get_httpd_uri_handler = handler;
	} else if(method == HTTP_POST) {
		lssl_custom_post_httpd_uri_handler = handler;
	} else {
		return ESP_ERR_INVALID_ARG;
	}
	return ESP_OK;
}

