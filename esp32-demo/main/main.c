#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#define ESP_WIFI_SSID      "esp32-ap"
#define ESP_WIFI_PASS      "00000000"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       2

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define GTK_REKEY_INTERVAL 0
#endif

static const char *TAG = "main.c";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
// static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP Start");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "AP Stop");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    // else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    //     xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    // } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    //     esp_wifi_connect();
//         xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
//     } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
//         ESP_LOGI(TAG, "Scan done");
//     } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
//         ESP_LOGI(TAG, "Found channel");
//     } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
//         ESP_LOGI(TAG, "Got SSID and password");

//         smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
//         wifi_config_t wifi_config;
//         uint8_t ssid[33] = { 0 };
//         uint8_t password[65] = { 0 };
//         uint8_t rvd_data[33] = { 0 };

//         bzero(&wifi_config, sizeof(wifi_config_t));
//         memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
//         memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

// #ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
//         wifi_config.sta.bssid_set = evt->bssid_set;
//         if (wifi_config.sta.bssid_set == true) {
//             ESP_LOGI(TAG, "Set MAC address of target AP: "MACSTR" ", MAC2STR(evt->bssid));
//             memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
//         }
// #endif

//         memcpy(ssid, evt->ssid, sizeof(evt->ssid));
//         memcpy(password, evt->password, sizeof(evt->password));
//         ESP_LOGI(TAG, "SSID:%s", ssid);
//         ESP_LOGI(TAG, "PASSWORD:%s", password);
//         if (evt->type == SC_TYPE_ESPTOUCH_V2) {
//             ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
//             ESP_LOGI(TAG, "RVD_DATA:");
//             for (int i=0; i<33; i++) {
//                 printf("%02x ", rvd_data[i]);
//             }
//             printf("\n");
//         }

//         ESP_ERROR_CHECK( esp_wifi_disconnect() );
//         ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
//         esp_wifi_connect();
//     } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
//         xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
//     }
}

static int check_wifi_sta_or_ap(void)
{
    wifi_config_t wifi_config;
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &wifi_config));
    ESP_LOGI(TAG, "AP SSID:%s", wifi_config.ap.ssid);
    ESP_LOGI(TAG, "AP PASSWORD:%s", wifi_config.ap.password);

    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "STA SSID:%s", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "STA PASSWORD:%s", wifi_config.sta.password);

    return 1;
}

static void initialise_wifi(void)
{
    int ret = 0;
    ESP_ERROR_CHECK(esp_netif_init());
    // s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    // ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    // ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ret = check_wifi_sta_or_ap();
    if (ret == 1) {
        esp_netif_create_default_wifi_ap();

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = ESP_WIFI_SSID,
                .ssid_len = strlen(ESP_WIFI_SSID),
                .channel = ESP_WIFI_CHANNEL,
                .password = ESP_WIFI_PASS,
                .max_connection = MAX_STA_CONN,
    #ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
                .authmode = WIFI_AUTH_WPA3_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
    #else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
                .authmode = WIFI_AUTH_WPA2_PSK,
    #endif
                .pmf_cfg = {
                        .required = true,
                },
    #ifdef CONFIG_ESP_WIFI_BSS_MAX_IDLE_SUPPORT
                .bss_max_idle_cfg = {
                    .period = WIFI_AP_DEFAULT_MAX_IDLE_PERIOD,
                    .protected_keep_alive = 1,
                },
    #endif
                .gtk_rekey_interval = GTK_REKEY_INTERVAL,
            },
        };

        if (strlen(ESP_WIFI_PASS) == 0) {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        if (wifi_config.ap.ssid_len > 32) {
            wifi_config.ap.ssid_len = 32;
            ESP_LOGW(TAG, "SSID too long, use first 32 bytes");
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

        ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
                ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
    } else {
        // esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        // assert(sta_netif);

        // ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    }

    ESP_ERROR_CHECK( esp_wifi_start() );

}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialise_wifi();
}
