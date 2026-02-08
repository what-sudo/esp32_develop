#ifndef __MAIN_H__
#define __MAIN_H__

typedef struct {
    uint8_t wifi_connect_status;
    uint8_t mac_addr_sta[6];
    uint8_t mac_addr_ap[6];
    uint8_t mac_addr_ble[6];
    uint8_t mac_addr_eth[6];
} system_status_t;

extern int g_clean_wifi_info_flag;
extern system_status_t g_system_status;

int write_wifi_info(char *ssid, char *password);

#endif
