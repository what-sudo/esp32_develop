#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_log.h"

#include "main.h"
#include "user_nvs_rw.h"

static const char *TAG = "user_nvs_rw.c";

typedef struct {
    nvs_type_t type;
    const char *str;
} type_str_pair_t;

static const type_str_pair_t type_str_pair[] = {
    { NVS_TYPE_I8, "i8" },
    { NVS_TYPE_U8, "u8" },
    { NVS_TYPE_U16, "u16" },
    { NVS_TYPE_I16, "i16" },
    { NVS_TYPE_U32, "u32" },
    { NVS_TYPE_I32, "i32" },
    { NVS_TYPE_U64, "u64" },
    { NVS_TYPE_I64, "i64" },
    { NVS_TYPE_STR, "str" },
    { NVS_TYPE_BLOB, "blob" },
    { NVS_TYPE_ANY, "any" },
};

static const size_t TYPE_STR_PAIR_SIZE = sizeof(type_str_pair) / sizeof(type_str_pair[0]);

static const char *type_to_str(nvs_type_t type)
{
    for (int i = 0; i < TYPE_STR_PAIR_SIZE; i++) {
        const type_str_pair_t *p = &type_str_pair[i];
        if (p->type == type) {
            return  p->str;
        }
    }

    return "Unknown";
}

int user_nvs_write_bemfa_info(char *namespace, char *ssid, char *password, char *token)
{
    ESP_LOGW(TAG, "NVS write namespace:%s bemfa info ssid:%s pass:%s token:%s", namespace, ssid, password, token);

    // Open NVS handle
    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }

    ret = nvs_set_str(my_handle, NVS_BIND_SSID, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write namespace:%s key:%s value:%s", namespace, NVS_BIND_SSID, ssid);
    }

    ret = nvs_set_str(my_handle, NVS_BIND_PASS, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write namespace:%s key:%s value:%s", namespace, NVS_BIND_PASS, password);
    }

    ret = nvs_set_str(my_handle, NVS_BEMFA_TOKEN, token);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write namespace:%s key:%s value:%s", namespace, NVS_BEMFA_TOKEN, token);
    }

    ESP_LOGI(TAG, "Committing updates in NVS...");
    ret = nvs_commit(my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }

    // Close
    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");

    return 0;
}


int user_nvs_read_bemfa_info(char *namespace, char *ssid, size_t ssid_size, char *password, size_t password_size, char *token, size_t token_size)
{
    ESP_LOGW(TAG, "NVS Read namespace:%s bemfa info ", namespace);

    // Open NVS handle
    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }
    ret = nvs_get_str(my_handle, NVS_BIND_SSID, ssid, &ssid_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read namespace:%s key:%s", namespace, NVS_BIND_SSID);
    }

    ret = nvs_get_str(my_handle, NVS_BIND_PASS, password, &password_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read namespace:%s key:%s", namespace, NVS_BIND_PASS);
    }

    ret = nvs_get_str(my_handle, NVS_BEMFA_TOKEN, token, &token_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read namespace:%s key:%s", namespace, NVS_BEMFA_TOKEN);
    }

    // Close
    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");

    return 0;
}

int user_nvs_write_int8(char *namespace, char *key, int8_t value)
{
    ESP_LOGI(TAG, "NVS write namespace:%s key:%s i8 value:%d", namespace, key, value);

    // Open NVS handle
    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }

    ret = nvs_set_i8(my_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write namespace:%s key:%s value:%d", namespace, key, value);
    }

    ESP_LOGI(TAG, "Committing updates in NVS...");
    ret = nvs_commit(my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }

    // Close
    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");

    return 0;
}

int user_nvs_write_string(char *namespace, char *key, char *value)
{
    ESP_LOGW(TAG, "NVS write namespace:%s key:%s str value:%s", namespace, key, value);

    // Open NVS handle
    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(namespace, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }

    ret = nvs_set_str(my_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write namespace:%s key:%s value:%s", namespace, key, value);
    }

    ESP_LOGI(TAG, "Committing updates in NVS...");
    ret = nvs_commit(my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }

    // Close
    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");

    return 0;
}

int dump_nvs_key_value(char *namespace)
{
    ESP_LOGW(TAG, "[%s] NVS dump namespace:%s", __func__, namespace);

    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(namespace, NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }

    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find("nvs", namespace, NVS_TYPE_ANY, &it);
    while(res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        const char *type_str =  type_to_str(info.type);
        int type = info.type;
        switch (type) {
            case NVS_TYPE_U8  : {
                uint8_t value = 0;
                nvs_get_u8(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_I8  : {
                int8_t value = 0;
                nvs_get_i8(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_U16 : {
                uint16_t value = 0;
                nvs_get_u16(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_I16 : {
                int16_t value = 0;
                nvs_get_i16(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_U32 : {
                uint32_t value = 0;
                nvs_get_u32(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_I32 : {
                int32_t value = 0;
                nvs_get_i32(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_U64 : {
                uint64_t value = 0;
                nvs_get_u64(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_I64 : {
                int64_t value = 0;
                nvs_get_i64(my_handle, info.key, &value);
                ESP_LOGI(TAG, "Key: '%s', Type: %s value: %d", info.key, type_str, value);
            } break;
            case NVS_TYPE_STR : {
                size_t required_size = 0;
                ret = nvs_get_str(my_handle, info.key, NULL, &required_size);
                if (ret == ESP_OK) {
                    char* message = malloc(required_size);
                    ret = nvs_get_str(my_handle, info.key, message, &required_size);
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "Key: '%s', Type: %s val:%s", info.key, type_str, message);
                    }
                    free(message);
                }
            } break;
            case NVS_TYPE_BLOB:
            default:
                ESP_LOGI(TAG, "Key: '%s', Type: %s ...", info.key, type_str);
                break;
        }

        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);

    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");

    return 0;
}

int user_nvs_init(void)
{
    int8_t system_reset_counter = 0;
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    do {
        // Open NVS handle
        nvs_handle_t my_handle;
        ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
            ret = -1;
            break;
        }

        // Read back the value
        ret = nvs_get_i8(my_handle, NVS_RST_CNT_KEY, &system_reset_counter);
        switch (ret) {
            case ESP_OK:
                ESP_LOGI(TAG, "Read sys reset counter = %d", system_reset_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGW(TAG, "The value is not initialized yet!");
                break;
            default:
                ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret));
        }

        if (system_reset_counter >= 5) {
            g_clean_wifi_info_flag = 1;

            ESP_LOGI(TAG, "Write sys reset counter = 0");
            ret = nvs_set_i8(my_handle, NVS_RST_CNT_KEY, 0);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write counter!");
            }
        } else {
            system_reset_counter++;
            ESP_LOGI(TAG, "Write sys reset counter = %d", system_reset_counter);
            ret = nvs_set_i8(my_handle, NVS_RST_CNT_KEY, system_reset_counter);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write counter!");
            }
        }

        ESP_LOGI(TAG, "Committing updates in NVS...");
        ret = nvs_commit(my_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes!");
        }

        // Close
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS handle closed.");

    } while (0);

    return ret;
}
