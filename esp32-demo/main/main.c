#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "main.c";

#define ENABLE_SMARTCONFIG 1
#if ENABLE_SMARTCONFIG
#include "esp_smartconfig.h"
int g_smartconfig_enable = 0;
#endif // ENABLE_SMARTCONFIG

// AP
#define ESP_AP_WIFI_SSID      "esp32-ap"
#define ESP_AP_WIFI_PASS      "00000000"
#define ESP_AP_WIFI_CHANNEL   1
#define MAX_STA_CONN       2

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define GTK_REKEY_INTERVAL CONFIG_ESP_GTK_REKEY_INTERVAL
#else
#define GTK_REKEY_INTERVAL 0
#endif

// STA
#define ESP_PREWRITE_WIFI      0
#if ESP_PREWRITE_WIFI
#define ESP_STA_WIFI_SSID      "ZTE-xNH5kA"
#define ESP_STA_WIFI_PASS      "rwYwsWh1P3"
#endif
#define ESP_STA_MAXIMUM_RETRY  10

// NVS
#define NVS_NAMESPACE          "storage"
#define NVS_RST_CNT_KEY        "rst_cnt"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT           BIT0
#define WIFI_FAIL_BIT                BIT1
#define WIFI_SMARTCONFIG_DONE_BIT    BIT2
#define SYS_RESTORE_TIMEOUT_BIT      BIT3

static int s_retry_num = 0;
static int g_clean_wifi_info_flag = 0;
static wifi_config_t g_wifi_sta_config;

static esp_timer_handle_t system_restore_time_handle;

static void smartconfig_task(void * parm);

static int print_system_info(void)
{
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "reset reason: %d", reset_reason);

    uint8_t mac_addr_base[6];
    uint8_t mac_addr_sta[6];
    uint8_t mac_addr_ap[6];
    uint8_t mac_addr_ble[6];
    uint8_t mac_addr_eth[6];

    esp_read_mac(mac_addr_base, ESP_MAC_BASE);
    esp_read_mac(mac_addr_sta, ESP_MAC_WIFI_STA);
    esp_read_mac(mac_addr_ap, ESP_MAC_WIFI_SOFTAP);
    esp_read_mac(mac_addr_ble, ESP_MAC_BT);
    esp_read_mac(mac_addr_eth, ESP_MAC_ETH);

    ESP_LOGI(TAG, "mac_addr_base:"MACSTR, MAC2STR(mac_addr_base));
    ESP_LOGI(TAG, "mac_addr_sta:"MACSTR, MAC2STR(mac_addr_sta));
    ESP_LOGI(TAG, "mac_addr_ap :"MACSTR, MAC2STR(mac_addr_ap));
    ESP_LOGI(TAG, "mac_addr_ble:"MACSTR, MAC2STR(mac_addr_ble));
    ESP_LOGI(TAG, "mac_addr_eth:"MACSTR, MAC2STR(mac_addr_eth));

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    printf("silicon revision v%d.%d\n", chip_info.revision / 100, chip_info.revision % 100);

    uint32_t flash_size;
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
    }

     ESP_LOGI(TAG, "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    uint32_t free_heap_size = esp_get_free_heap_size();
    ESP_LOGI(TAG, "free_heap_size: %d", free_heap_size);
    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

    return 0;
}

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
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA Start");
#if ENABLE_SMARTCONFIG
        if (g_smartconfig_enable) {
            ESP_LOGI(TAG, "SmartConfig start");
            xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
        } else {
            esp_wifi_connect();
        }
#else
        esp_wifi_connect();
#endif // ENABLE_SMARTCONFIG
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        ESP_LOGI(TAG, "STA Stop");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_STA_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        uint8_t rvd_data[33] = { 0 };

        memset(&g_wifi_sta_config, 0, sizeof(wifi_config_t));
        memcpy(g_wifi_sta_config.sta.ssid, evt->ssid, sizeof(g_wifi_sta_config.sta.ssid));
        memcpy(g_wifi_sta_config.sta.password, evt->password, sizeof(g_wifi_sta_config.sta.password));

        ESP_LOGI(TAG, "SSID:%s", g_wifi_sta_config.sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", g_wifi_sta_config.sta.password);

        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &g_wifi_sta_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_SMARTCONFIG_DONE_BIT);
    }
}

static int clean_reset_counter(void)
{
     // Open NVS handle
    nvs_handle_t my_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "Write sys reset counter = 0");
    ret = nvs_set_u8(my_handle, NVS_RST_CNT_KEY, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write counter!");
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

static int user_nvs_init(void)
{
    uint8_t system_reset_counter = 0;
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
        ret = nvs_get_u8(my_handle, NVS_RST_CNT_KEY, &system_reset_counter);
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

        system_reset_counter++;
        ESP_LOGI(TAG, "Write sys reset counter = %d", system_reset_counter);
        ret = nvs_set_u8(my_handle, NVS_RST_CNT_KEY, system_reset_counter);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write counter!");
        }

        ESP_LOGI(TAG, "Committing updates in NVS...");
        ret = nvs_commit(my_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes!");
        }

        // Close
        nvs_close(my_handle);
        ESP_LOGI(TAG, "NVS handle closed.");

        if (system_reset_counter >= 5) {
            g_clean_wifi_info_flag = 1;
            clean_reset_counter();
        }
    } while (0);

    return ret;
}

static int check_wifi_sta_or_ap(void)
{
    int ret = 0;
    wifi_config_t wifi_ap_config;

    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_LOGI(TAG, "AP SSID:%s", wifi_ap_config.ap.ssid);
    ESP_LOGI(TAG, "AP PASSWORD:%s", wifi_ap_config.ap.password);

    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &g_wifi_sta_config));
    ESP_LOGI(TAG, "STA SSID:%s", g_wifi_sta_config.sta.ssid);
    ESP_LOGI(TAG, "STA PASSWORD:%s", g_wifi_sta_config.sta.password);

#if ESP_PREWRITE_WIFI
    wifi_config_t wifi_sta_config = {
        .sta.ssid = ESP_STA_WIFI_SSID,
        .sta.password = ESP_STA_WIFI_PASS,
    };

    ESP_LOGW(TAG, "wriet wifi ssid:%s pass:%s", wifi_sta_config.sta.ssid, wifi_sta_config.sta.password);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &g_wifi_sta_config));
#endif

    if (strlen((char*)g_wifi_sta_config.sta.ssid) > 0) {
        ret = 0;
    } else {
        ret = 1;
#if ENABLE_SMARTCONFIG
        g_smartconfig_enable = 1;
        ret = 0;
#endif // ENABLE_SMARTCONFIG
    }

    if (g_clean_wifi_info_flag == 1) {
        wifi_config_t wifi_config = { 0 };
        ESP_LOGW(TAG, "Clean wifi info, restarting...");
        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_restart();
    }

    return ret;
}

static void initialise_wifi(void)
{
    int ret = 0;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );

#if ENABLE_SMARTCONFIG
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
#endif // ENABLE_SMARTCONFIG

    ret = check_wifi_sta_or_ap();
    if (ret == 1) {
        ESP_LOGI(TAG, "==========>>> ESP_WIFI_MODE_AP");
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_ap();
        assert(sta_netif);

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = ESP_AP_WIFI_SSID,
                .ssid_len = strlen(ESP_AP_WIFI_SSID),
                .channel = ESP_AP_WIFI_CHANNEL,
                .password = ESP_AP_WIFI_PASS,
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

        if (strlen(ESP_AP_WIFI_PASS) == 0) {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        if (wifi_config.ap.ssid_len > 32) {
            wifi_config.ap.ssid_len = 32;
            ESP_LOGW(TAG, "SSID too long, use first 32 bytes");
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
                ESP_AP_WIFI_SSID, ESP_AP_WIFI_PASS, ESP_AP_WIFI_CHANNEL);
    } else {
        ESP_LOGI(TAG, "==========>>> ESP_WIFI_MODE_STA");
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        assert(sta_netif);

        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK( esp_wifi_start() );
        ESP_LOGI(TAG, "wifi_init_sta finished.");
    }
}

static void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_SMARTCONFIG_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & WIFI_SMARTCONFIG_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            break;
        }
    }

    vTaskDelete(NULL);
}

static void system_restore_timer_callback(void* arg)
{
    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGI(TAG, "system_restore_timer, time since boot: %lld us", time_since_boot);\

    clean_reset_counter();

    xEventGroupSetBits(s_wifi_event_group, SYS_RESTORE_TIMEOUT_BIT);
}

static int user_timer_init(void)
{
    const esp_timer_create_args_t system_restore_timeout = {
            .callback = &system_restore_timer_callback,
            .name = "system_restore_timer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&system_restore_timeout, &system_restore_time_handle));

    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_once(system_restore_time_handle, 5000000));
    ESP_LOGI(TAG, "Started timers, time since boot: %lld us", esp_timer_get_time());

    return 0;
}

void app_main(void)
{
    print_system_info();

    s_wifi_event_group = xEventGroupCreate();

    user_nvs_init();
    user_timer_init();

    initialise_wifi();

    while (1)
    {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | SYS_RESTORE_TIMEOUT_BIT,
                pdTRUE,
                pdFALSE,
                portMAX_DELAY);
        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
        * happened. */
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                    g_wifi_sta_config.sta.ssid, g_wifi_sta_config.sta.password);
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                    g_wifi_sta_config.sta.ssid, g_wifi_sta_config.sta.password);
        } else if (bits & SYS_RESTORE_TIMEOUT_BIT) {
            ESP_LOGI(TAG, "System restore timeout");
            esp_timer_stop(system_restore_time_handle);
            ESP_ERROR_CHECK(esp_timer_delete(system_restore_time_handle));
            ESP_LOGI(TAG, "Stopped and deleted system_restore_times");
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
}
