/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <sys/param.h>
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_tls.h"
#include "esp_check.h"
#include <time.h>
#include <sys/time.h>

#include "main.h"
#include "user_http_server.h"

static const char *TAG = "user_httpd";

static httpd_handle_t g_httpd_server = NULL;
static int g_pre_start_mem, g_post_stop_mem;

extern const char _binary_index_html_start[] asm("_binary_index_html_start");
extern const char _binary_index_html_end[]   asm("_binary_index_html_end");

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Free Stack for server task: '%d'", uxTaskGetStackHighWaterMark(NULL));

    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
    size_t len = _binary_index_html_end - _binary_index_html_start;
    httpd_resp_send(req, _binary_index_html_start, len);
    return ESP_OK;
}

static esp_err_t echo_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/echo handler read content length %d", req->content_len);

    char*  buf = malloc(req->content_len + 1);
    size_t off = 0;
    int    ret;

    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory of %d bytes!", req->content_len + 1);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (off < req->content_len) {
        /* Read data received in the request */
        ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free (buf);
            return ESP_FAIL;
        }
        off += ret;
        ESP_LOGI(TAG, "/echo handler recv length %d", ret);
    }
    buf[off] = '\0';

    if (req->content_len < 128) {
        ESP_LOGI(TAG, "/echo handler read %s", buf);
    }

    /* Search for Custom header field */
    char*  req_hdr = 0;
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Custom");
    if (hdr_len) {
        /* Read Custom header value */
        req_hdr = malloc(hdr_len + 1);
        if (!req_hdr) {
            ESP_LOGE(TAG, "Failed to allocate memory of %d bytes!", hdr_len + 1);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        httpd_req_get_hdr_value_str(req, "Custom", req_hdr, hdr_len + 1);

        /* Set as additional header for response packet */
        httpd_resp_set_hdr(req, "Custom", req_hdr);
    }
    httpd_resp_send(req, buf, req->content_len);
    free (req_hdr);
    free (buf);
    return ESP_OK;
}

static const httpd_uri_t basic_handlers[] = {
    { .uri      = "/",
      .method   = HTTP_GET,
      .handler  = hello_get_handler,
      .user_ctx = NULL,
    },
    { .uri      = "/echo",
      .method   = HTTP_POST,
      .handler  = echo_post_handler,
      .user_ctx = NULL,
    }
};

static const int basic_handlers_no = sizeof(basic_handlers)/sizeof(httpd_uri_t);
static void register_basic_handlers(httpd_handle_t hd)
{
    int i;
    ESP_LOGI(TAG, "Registering basic handlers");
    ESP_LOGI(TAG, "No of handlers = %d", basic_handlers_no);
    for (i = 0; i < basic_handlers_no; i++) {
        if (httpd_register_uri_handler(hd, &basic_handlers[i]) != ESP_OK) {
            ESP_LOGW(TAG, "register uri failed for %d", i);
            return;
        }
    }
    ESP_LOGI(TAG, "Success");
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    char response[128];
    strncpy(response, req->uri, sizeof(response));

    snprintf(response + strlen(req->uri), sizeof(response) - strlen(req->uri), " URI is not available");
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, response);
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Modify this setting to match the number of test URI handlers */
    config.max_uri_handlers  = 9;
    config.server_port = 80;

    /* This check should be a part of http_server */
    config.max_open_sockets = (CONFIG_LWIP_MAX_SOCKETS - 3);

    g_pre_start_mem = esp_get_free_heap_size();
    ESP_LOGI(TAG, "HTTPD Start: Current free memory: %d", g_pre_start_mem);

    // Start the httpd server
    if (httpd_start(&server, &config) == ESP_OK) {

        ESP_LOGI(TAG, "Started HTTP server on port: '%d'", config.server_port);
        ESP_LOGI(TAG, "Max URI handlers: '%d'", config.max_uri_handlers);
        ESP_LOGI(TAG, "Max Open Sessions: '%d'", config.max_open_sockets);
        ESP_LOGI(TAG, "Max Header Length: '%d'", CONFIG_HTTPD_MAX_REQ_HDR_LEN);
        ESP_LOGI(TAG, "Max URI Length: '%d'", CONFIG_HTTPD_MAX_URI_LEN);
        ESP_LOGI(TAG, "Max Stack Size: '%d'", config.stack_size);

        register_basic_handlers(server);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    if (server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(server);

    g_post_stop_mem = esp_get_free_heap_size();
    ESP_LOGI(TAG, "HTTPD Stop: Current free memory: %d", g_post_stop_mem);

    // Stop the httpd server
    return err;
}

void user_http_server_task(void *pvParameters)
{
    g_httpd_server = start_webserver();

    while (1)
    {
        if (g_system_status.wifi_connect_status == 0) {
            stop_webserver(g_httpd_server);
            g_httpd_server = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
