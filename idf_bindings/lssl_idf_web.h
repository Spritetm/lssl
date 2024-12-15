#include "esp_http_server.h"
#include <stdint.h>

typedef void (*program_changed_cb_t)(const char *programname);

//note: register the cb before lssl_web_init
void lssl_web_register_program_changed_cb(program_changed_cb_t cb);
void lssl_web_init(int led_count);
esp_err_t lssl_web_get_handler(httpd_req_t *req);
esp_err_t lssl_web_post_handler(httpd_req_t *req);
esp_err_t lssl_http_app_set_handler_hook(httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r));
uint8_t *lssl_web_get_program(const char *name, size_t *len);