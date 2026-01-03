#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"

#include "cJSON.h"

#include "main.h"
#include "user_nvs_rw.h"
#include "bemfa.h"

#include "user_http_client.h"

static const char *TAG = "bemfa.c";
static char g_bemfa_topic[32] = {0};
static char g_bemfa_token[64] = {0};

int parse_bemfa_bind_message(char *rx_buf, char *tx_buf)
{
    int ret = 0;

    int ssid_vaild = 0;
    int pass_vaild = 0;
    int token_vaild = 0;

    do {
        cJSON *root = cJSON_Parse(rx_buf);
        if (root == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                printf("Error before: %s\n", error_ptr);
            }
            ret = -1;
            break;
        }

        // 读取数值字段
        cJSON *cmdType = cJSON_GetObjectItemCaseSensitive(root, "cmdType");
        if (cJSON_IsNumber(cmdType)) {
            printf("cmdType: %d\n", cmdType->valueint);
        }

        // 读取字符串字段
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
        if (cJSON_IsString(ssid) && (ssid->valuestring != NULL)) {
            printf("ssid: %s\n", ssid->valuestring);
            ssid_vaild = 1;
        }

        // 读取字符串字段
        cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
        if (cJSON_IsString(password) && (password->valuestring != NULL)) {
            printf("password: %s\n", password->valuestring);
            pass_vaild = 1;
        }

        // 读取字符串字段
        cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "token");
        if (cJSON_IsString(token) && (token->valuestring != NULL)) {
            printf("token: %s\n", token->valuestring);
            token_vaild = 1;
        }

        if (cmdType->valueint == 1 && ssid_vaild && pass_vaild && token_vaild) {
            char topic[40] = {0};

            snprintf(topic, sizeof(topic), "esp32switch%x%x006", (unsigned int)(g_system_status.mac_addr_sta[4]), (unsigned int)(g_system_status.mac_addr_sta[5]));
            user_nvs_write_string(NVS_NAMESPACE, NVS_BEMFA_TOPIC, topic);

            user_nvs_write_bemfa_info(NVS_NAMESPACE, ssid->valuestring, password->valuestring, token->valuestring);

            sprintf(tx_buf, "{\"cmdType\":2,\"productId\":\"%s\",\"deviceName\":\"esp32_test\",\"protoVersion\":\"3.1\"}", topic);
            ret = strlen(tx_buf);

        } else if (cmdType->valueint == 3) {
            char ssid[33] = {0};
            char pass[33] = {0};
            char token[40] = {0};
            user_nvs_read_bemfa_info(NVS_NAMESPACE, ssid, sizeof(ssid), pass, sizeof(pass), token, sizeof(token));
            write_wifi_info(ssid, pass);

            ESP_LOGW(TAG, "esp restart");
            esp_restart();
            ret = 0;
        }

        cJSON_Delete(root); // 必须释放内存！
    } while (0);

    return ret;
}

int bemfa_device_addTopic(void)
{
    int ret = 0;
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

    esp_http_client_config_t config = {
        .url = BEMFA_DEVICE_ADDTOPIC_API,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[256] = {0};

    snprintf(post_data, sizeof(post_data), "{\"uid\":\"%s\",\"topic\":\"%s\",\"type\":3,\"wifiConfig\":1}", g_bemfa_token, g_bemfa_topic);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "HTTP POST body = %s", local_response_buffer);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);

    return ret;
}


void user_bemfa_connect_task(void *pvParameters)
{
    int ret = 0;
    int status = 0;

    user_nvs_read_string(NVS_NAMESPACE, NVS_BEMFA_TOPIC, g_bemfa_topic, sizeof(g_bemfa_topic));
    user_nvs_read_string(NVS_NAMESPACE, NVS_BEMFA_TOKEN, g_bemfa_token, sizeof(g_bemfa_token));

    while (1)
    {
        if (g_system_status.wifi_connect_status == 0) {
            status = 0;
            break;
        }

        switch (status)
        {
            case 0: {
                ret = bemfa_device_addTopic();
                if (ret == 0) {
                    status = 1;
                }
            } break;
            default:
                break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
