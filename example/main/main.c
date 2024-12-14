#include <stdio.h>
#include "esp_wifi.h"
#include "lssl_idf_web.h"
#include "wifi_manager.h"
#include "http_app.h"
#include "leds.h"
#include "led_syscalls.h"
#include "led_map.h"

#define EXTERN_FILE(name) \
	extern const char name##_start[] asm("_binary_"#name"_start"); \
	extern const long name##_length asm(#name"_length")

EXTERN_FILE(root_html);
EXTERN_FILE(root_css);
EXTERN_FILE(map_html);
EXTERN_FILE(favicon_ico);


typedef struct {
	char buf[128];
	int n;
} parse_float_t;

int parse_float(parse_float_t *s, char c, float *ret) {
	if (c=='\n' || c==',') {
		s->buf[s->n]=0;
		s->n=0;
		*ret=atof(s->buf);
		return 1;
	} else {
		s->buf[s->n]=c;
		if (s->n<127) s->n++;
	}
	return 0;
}


static esp_err_t handle_mapdata_post_req(httpd_req_t *req) {
	char *q=strchr(req->uri, '?');
	if (!q) goto err;
	q++; //skip ?
	esp_err_t r;
	char buf[128];
	r=httpd_query_key_value(q, "count", buf, 128);
	if (r!=ESP_ERR_NOT_FOUND) {
		int count=atoi(buf);
		httpd_query_key_value(q, "dim", buf, 128);
		int dim=atoi(buf);
		printf("New map: %d leds, %d dimensions\n", count, dim);
		int ret;
		//we're parsing the led number as a float but that's OK:
		//integers can be represented without loss from 0 to 16777216
		float point[4];
		led_map_create(count, dim);
		int led=0;
		int p=0;
		parse_float_t pf={};
		while ((ret = httpd_req_recv(req, buf, sizeof(buf)))>0) {
			float f;
			for (int i=0; i<ret; i++) {
				if (parse_float(&pf, buf[i], &f)) {
					point[p++]=f;
					if (p>=dim+1) {
						p=0;
						printf("Led %d: %f %f %f\n", (int)point[0], point[1], point[2], point[3]);
						led_map_set_led_pos((int)point[0], point[1], point[2], point[3]);
					}
				}
			}
		}
	}
	httpd_resp_send(req, "OK", 2);
	return ESP_OK;
err:
	httpd_resp_send(req, "?", 1);
	return ESP_OK;
}

static esp_err_t handle_mapdata_req(httpd_req_t *req) {
	char buf[128];
	sprintf(buf, "{\n\"count\": %d,\n \"pos\": [\n", led_map_get_size());
	httpd_resp_send_chunk(req, buf, strlen(buf));
	float x=0, y=0, z=0;
	int size=led_map_get_size();
	for (int i=0; i<size; i++) {
		led_map_get_led_pos(i, &x, &y, &z);
		sprintf(buf, "[%f, %f, %f]%s\n", x, y, z, i!=size-1?",":"");
		httpd_resp_send_chunk(req, buf, strlen(buf));
	}
	sprintf(buf, "]\n}\n");
	httpd_resp_send_chunk(req, buf, strlen(buf));

	httpd_resp_send_chunk(req, buf, 0); //finish
	return ESP_OK;
}

static esp_err_t handle_mapper_req(httpd_req_t *req) {
	char *q=strchr(req->uri, '?');
	if (!q) {
		httpd_resp_send(req, "?", 1);
		return ESP_OK;
	}
	q++; //skip ?
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

static esp_err_t post_handler(httpd_req_t *req) {
	if (strncmp(req->uri, "/mapdata", 7)==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/plain");
		return handle_mapdata_post_req(req);
	}
	printf("No POST handler found for '%s'!\n", req->uri);
	httpd_resp_set_status(req, "404 Not Found");
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_send(req, "Not Found", 9);
	return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req) {
	if (strcmp(req->uri, "/root.html")==0 || strcmp(req->uri, "/")==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, root_html_start, root_html_length);
		return ESP_OK;
	}
	if (strcmp(req->uri, "/map.html")==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, map_html_start, map_html_length);
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
		return handle_mapper_req(req);
	}
	if (strncmp(req->uri, "/mapdata", 8)==0) {
		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/json");
		return handle_mapdata_req(req);
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
	lssl_http_app_set_handler_hook(HTTP_POST, &post_handler);
	lssl_web_register_program_changed_cb(prog_changed);
	leds_init(8);
}
