#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"

#include "cJSON.h"

#include "main.h"
#include "user_nvs_rw.h"

static const char *TAG = "bemfa.c";

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
            user_nvs_write_bemfa_info(NVS_NAMESPACE, ssid->valuestring, password->valuestring, token->valuestring);

            sprintf(tx_buf, "{\"cmdType\":2,\"productId\":\"esp32switch006\",\"deviceName\":\"esp32_test\",\"protoVersion\":\"3.1\"}");
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
