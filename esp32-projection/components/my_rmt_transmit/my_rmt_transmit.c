#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "my_rmt_transmit.h"
#include "ir_nec_encoder.h"

const static char *TAG = "my_rmt_transmit.c";


#define EXAMPLE_IR_RESOLUTION_HZ     1000000 // 1MHz resolution, 1 tick = 1us
#define EXAMPLE_IR_TX_GPIO_NUM       18
#define EXAMPLE_IR_RX_GPIO_NUM       19
#define EXAMPLE_IR_NEC_DECODE_MARGIN 200     // Tolerance for parsing RMT symbols into bit stream

rmt_channel_handle_t tx_channel = NULL;
rmt_encoder_handle_t nec_encoder = NULL;

// this example won't send NEC frames in a loop
rmt_transmit_config_t transmit_config = {
    .loop_count = 0, // no loop
};

static void user_rmt_transmit_task(void *pvParameters)
{
    while (1) {
        // timeout, transmit predefined IR NEC packets
        const ir_nec_scan_code_t scan_code = {
            .address = 0x0ff0,
            .command = 0x55aa,
        };
        ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &scan_code, sizeof(scan_code), &transmit_config));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void rmt_transmit_init(void)
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,   // 选择时钟源
        .gpio_num = 3,                    // GPIO 编号
        .resolution_hz = 1 * 1000 * 1000, // 1 MHz 滴答分辨率，即 1 滴答 = 1 µs
        .mem_block_symbols = 64,          // 内存块大小，即 64 * 4 = 256 字节
        .trans_queue_depth = 4,           // 设置后台等待处理的事务数量
        .flags.invert_out = false,        // 不反转输出信号
        .flags.with_dma = false,          // 不需要 DMA 后端
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));

    rmt_carrier_config_t tx_carrier_cfg = {
        .duty_cycle = 0.33,                 // 载波占空比为 33%
        .frequency_hz = 38000,              // 38 KHz
        .flags.polarity_active_low = false, // 载波应调制到高电平
        .flags.always_on = false,
    };
    // 将载波调制到 TX 通道
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &tx_carrier_cfg));

    ESP_LOGI(TAG, "install IR NEC encoder");
    ir_nec_encoder_config_t nec_encoder_cfg = {
        .resolution = EXAMPLE_IR_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));

    ESP_LOGI(TAG, "enable RMT TX and RX channels");
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    ESP_LOGI(TAG, "RMT TX channel initialized successfully");

    xTaskCreate(user_rmt_transmit_task, "user_rmt_transmit", 4096, NULL, 5, NULL);
}
