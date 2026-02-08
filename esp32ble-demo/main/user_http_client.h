#ifndef __USER_HTTP_CLIENT_H__
#define __USER_HTTP_CLIENT_H__

#define MAX_HTTP_OUTPUT_BUFFER 512

#include "esp_http_client.h"
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

int user_http_get(char *url, char *response, int buf_len);

#endif
