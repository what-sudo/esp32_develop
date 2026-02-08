#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/ledc.h"

#include "pwm.h"

static const char *TAG = "pwm.c";

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (2) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_12_BIT // Set duty resolution to 13 bits
#define LEDC_FREQUENCY          (400) // Frequency in

static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .duty_resolution  = LEDC_DUTY_RES,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .timer_sel      = LEDC_TIMER,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void user_pwm_task(void *pvParameters)
{
    example_ledc_init();
    uint32_t duty = 2048;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    // uint32_t freq = 200;
    // int ret = 0;
    // while (1) {
    //     freq = ledc_get_freq(LEDC_MODE, LEDC_TIMER);
    //     ESP_LOGI(TAG, "freq: %d", freq);

    //     freq += 100;
    //     if (freq >= 10000) {
    //         freq = 100;
    //     }

    //     ret = ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq);
    //     if (ret != ESP_OK) {
    //         ESP_LOGE(TAG, "ledc_set_freq failed, ret=%d", ret);
    //     }

    //     vTaskDelay(100 / portTICK_PERIOD_MS);
    // }

    vTaskDelete(NULL);

}
