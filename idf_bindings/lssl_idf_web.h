#include "esp_http_server.h"

esp_err_t lssl_web_get_handler(httpd_req_t *req);
esp_err_t lssl_web_post_handler(httpd_req_t *req);
esp_err_t lssl_http_app_set_handler_hook(httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r));
