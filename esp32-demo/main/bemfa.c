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

#include "protocol.h"
#include "user_http_client.h"

static const char *TAG = "bemfa.c";
static char g_bemfa_topic[32] = {0};
static char g_bemfa_token[64] = {0};

static char g_bemfa_ipaddr[16] = {0};

static int g_bemfa_status = 0;
static int g_tcp_sock = 0;

static int g_bemfa_switch_status = 0;

// 辅助函数：移除字符串尾部的空白和控制字符（\r, \n, 空格, 制表符等）
static void trim_trailing_whitespace(char *str)
{
    if (!str) return;

    size_t len = strlen(str);
    if (len == 0) return;

    // 从末尾向前找第一个非空白字符
    while (len > 0) {
        unsigned char c = str[len - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            len--;
        } else {
            break;
        }
    }
    str[len] = '\0';
}

bool parse_query_value(const char *query, const char *key, char *out_val, size_t out_len)
{
    if (!query || !key || !out_val || out_len == 0) {
        return false;
    }

    // 构造要查找的子串： "key="
    char key_with_eq[64];
    int key_len = strlen(key);
    if (key_len + 2 >= sizeof(key_with_eq)) {
        return false; // key 太长
    }
    snprintf(key_with_eq, sizeof(key_with_eq), "%s=", key);

    // 在 query 中查找 "key="
    const char *p = strstr(query, key_with_eq);
    if (!p) {
        return false;
    }

    // 跳过 "key="
    p += strlen(key_with_eq);

    // 找到值的结束位置（遇到 & 或 \0）
    const char *end = strchr(p, '&');
    size_t val_len;
    if (end) {
        val_len = end - p;
    } else {
        val_len = strlen(p);
    }

    // 防止缓冲区溢出
    if (val_len >= out_len) {
        val_len = out_len - 1;
    }

    // 复制值
    memcpy(out_val, p, val_len);
    out_val[val_len] = '\0';

    // ✅ 关键：清理尾部的 \r\n、空格等
    trim_trailing_whitespace(out_val);
    return true;
}

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
    int ret = -1;
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

        ret = -2;
        cJSON *root = cJSON_Parse(local_response_buffer);
        if (root == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                printf("Error before: %s\n", error_ptr);
            }
        } else {
            ret = -3;
            cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
            if (cJSON_IsObject(data)) {
                cJSON *code = cJSON_GetObjectItemCaseSensitive(data, "code");
                if (cJSON_IsNumber(code)) {
                    if (code->valueint == 0 || code->valueint == 40006) {
                        ret = 0;
                    }
                }
            }
            cJSON_Delete(root);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);

    return ret;
}

int bemfa_device_subscribe(void)
{
    int ret = 0;
    // cmd=3&uid=6cf90baf69f846f08b5a8383d6256a49&topic=esp32switchea28006\r\n
    char subscribe_str[128] = {0};
    char rx_buffer[128] = {0};

    ret = -1;
    do {
        snprintf(subscribe_str, sizeof(subscribe_str), "cmd=3&uid=%s&topic=%s\r\n", g_bemfa_token, g_bemfa_topic);
        int err = send(g_tcp_sock, subscribe_str, strlen(subscribe_str), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }

        // cmd=3&uid=xxxxxxxxxxxxxxxxxxxxxxx&topic=light002&msg=on
        int len = recv(g_tcp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[%d] Timeout, no data received.\n", __LINE__);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            } else {
                perror("recv error");
                break;
            }
        } else if (len == 0) {
            printf("Server closed connection.\n");
            break;
        } else {
            printf("[%d] sub Received: %.*s\n", __LINE__, (int)len, rx_buffer);
            parse_query_value(rx_buffer, "cmd", subscribe_str, sizeof(subscribe_str));
            if (strcmp(subscribe_str, "3") == 0) {
                ret = 0;
                parse_query_value(rx_buffer, "msg", subscribe_str, sizeof(subscribe_str));
                printf("parse msg: |%s|\n", subscribe_str);
                if (strcmp(subscribe_str, "on") == 0) {
                    g_bemfa_switch_status = 1;
                } else {
                    g_bemfa_switch_status = 0;
                }
            } else {
                ESP_LOGE(TAG, "Not found cmd 3, retry");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }
        }
    } while (0);

    return ret;
}

int bemfa_device_public(char *msg)
{
    ESP_LOGI(TAG, "bemfa_device_public msg=%s", msg);
    // cmd=2&uid=xxxxxxxxxxxxxxxxxxxxxxx&topic=light002&msg=off\r\n
    char public_str[128] = {0};
    char rx_buffer[128] = {0};
    int ret = -1;
    do {
        snprintf(public_str, sizeof(public_str), "cmd=2&uid=%s&topic=%s&msg=%s\r\n", g_bemfa_token, g_bemfa_topic, msg);
        int err = send(g_tcp_sock, public_str, strlen(public_str), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }

        // cmd=2&res=1
        int len = recv(g_tcp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[%d] Timeout, no data received.\n", __LINE__);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            } else {
                perror("recv error");
                break;
            }
        } else if (len == 0) {
            printf("Server closed connection.\n");
            break;
        } else {
            printf("[%d] pub Received: %.*s\n", __LINE__, (int)len, rx_buffer);
            parse_query_value(rx_buffer, "cmd", public_str, sizeof(public_str));
            printf("parse cmd: |%s|\n", public_str);
            if (strcmp(public_str, "2") == 0) {
                ret = 0;
            }
        }
    } while (0);

    return ret;
}

int bemfa_device_listen(void)
{
    char rx_buffer[128] = {0};
    char parse[32] = {0};
    int ret = -1;
    do {
        int len = recv(g_tcp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[%d] Timeout, no data received.\n", __LINE__);
            } else {
                perror("recv error");
            }
            ret = 0;
        } else if (len == 0) {
            printf("Server closed connection.\n");
            break;
        } else {
            ret = 0;
            printf("[%d] listen Received: %.*s\n", __LINE__, (int)len, rx_buffer);

            parse_query_value(rx_buffer, "msg", parse, sizeof(parse));
            printf("parse msg: |%s|\n", parse);
            if (strcmp(parse, "on") == 0) {
                g_bemfa_switch_status = 1;
            } else {
                g_bemfa_switch_status = 0;
            }
        }
    } while (0);

    return ret;
}

void user_bemfa_connect_task(void *pvParameters)
{
    int ret = 0;

    user_nvs_read_string(NVS_NAMESPACE, NVS_BEMFA_TOPIC, g_bemfa_topic, sizeof(g_bemfa_topic));
    user_nvs_read_string(NVS_NAMESPACE, NVS_BEMFA_TOKEN, g_bemfa_token, sizeof(g_bemfa_token));

    while (1)
    {
        if (g_system_status.wifi_connect_status == 0) {
            g_bemfa_status = 0;
            tcp_client_deinit(g_tcp_sock);
            g_tcp_sock = -1;
            break;
        }

        switch (g_bemfa_status)
        {
            case 0: {
                ret = dns_lookup(BEMFA_SERVER_HOSTNAME, g_bemfa_ipaddr);
                if (ret == 0) {
                    ESP_LOGI(TAG, "bemfa server ip:%s", g_bemfa_ipaddr);
                    g_bemfa_status = 1;
                }
            } break;
            case 1: {
                ret = bemfa_device_addTopic();
                if (ret == 0) {
                    g_bemfa_status = 2;
                }
            } break;
            case 2: {
                g_tcp_sock = tcp_client_init(g_bemfa_ipaddr, BEMFA_SERVER_PORT);
                if (g_tcp_sock > 0) {
                    g_bemfa_status = 3;
                } else {
                    ESP_LOGE(TAG, "bemfa server connect fail, sock:%d", g_tcp_sock);
                }
            } break;
            case 3: {
                ret = bemfa_device_subscribe();
                if (ret == 0) {
                    g_bemfa_status = 4;
                } else {
                    tcp_client_deinit(g_tcp_sock);
                    g_tcp_sock = -1;
                    g_bemfa_status = 2;
                }
            } break;
            case 4: {
                ret = bemfa_device_public(g_bemfa_switch_status == 1 ? "on" : "off");
                if (ret == 0) {
                    g_bemfa_status = 5;
                } else {
                    tcp_client_deinit(g_tcp_sock);
                    g_tcp_sock = -1;
                    g_bemfa_status = 2;
                }
            } break;
            case 5: {
                ret = bemfa_device_listen();
                if (ret == 0) {
                    g_bemfa_status = 5;
                } else {
                    tcp_client_deinit(g_tcp_sock);
                    g_tcp_sock = -1;
                    g_bemfa_status = 2;
                }
            } break;
            default:
                break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
