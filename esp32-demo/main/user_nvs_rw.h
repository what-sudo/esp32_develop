#ifndef __USER_NVS_RW_H__
#define __USER_NVS_RW_H__

// NVS
#define NVS_NAMESPACE          "storage"
#define NVS_RST_CNT_KEY        "rst_cnt"
#define NVS_BIND_SSID          "bind_ssid"
#define NVS_BIND_PASS          "bind_pass"
#define NVS_BEMFA_TOKEN        "bemfa_token"

int user_nvs_init(void);
int dump_nvs_key_value(char *namespace);
int user_nvs_write_bemfa_info(char *namespace, char *ssid, char *password, char *token);
int user_nvs_read_bemfa_info(char *namespace, char *ssid, size_t ssid_size, char *password, size_t password_size, char *token, size_t token_size);

int user_nvs_write_int8(char *namespace, char *key, int8_t value);
int user_nvs_write_string(char *namespace, char *key, char *value);

#endif
