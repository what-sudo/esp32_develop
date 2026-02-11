#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "projection.h"
#include "led.h"

static const char *TAG = "projection.c";

#define MOTOR_OUTPUT_SS       4
#define MOTOR_OUTPUT_PIN_SEL  ((1ULL << MOTOR_OUTPUT_SS))

#define MOTOR_INPUT_LD        5
#define MOTOR_INPUT_FG        7
#define MOTOR_INPUT_SWITCH    9
#define MOTOR_INPUT_PIN_SEL  ((1ULL << MOTOR_INPUT_LD) | (1ULL << MOTOR_INPUT_FG) | (1ULL << MOTOR_INPUT_SWITCH))

#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t g_gpio_evt_queue = NULL;

static int g_motor_status = 0;

static uint32_t g_motor_r = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    if (gpio_num == MOTOR_INPUT_FG) {
        g_motor_r++;
        if (g_motor_r >= 400) {
            g_motor_r = 0;
        }
    } else {
        xQueueSendFromISR(g_gpio_evt_queue, &gpio_num, NULL);
    }
}

static void projection_gpio_interrupt_task(void* arg)
{
    uint32_t io_num;
    int status = 0;
    for (;;) {
        if (xQueueReceive(g_gpio_evt_queue, &io_num, portMAX_DELAY)) {
            status = gpio_get_level(io_num);
            if (status == 1) {
                continue;
            } else {
                vTaskDelay(10 / portTICK_PERIOD_MS); // debounce delay
                status = gpio_get_level(io_num);
            }
            if (status == 1) {
                continue;
            }
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, status);

            if (io_num == MOTOR_INPUT_LD) {
                ESP_LOGI(TAG, "motor input LD interrupt triggered");
                g_led_clolr = 0x00FF00; // green
            } else if (io_num == MOTOR_INPUT_SWITCH) {
                ESP_LOGI(TAG, "motor input SWITCH interrupt triggered");
                if (g_motor_status == 0) {
                    gpio_set_level(MOTOR_OUTPUT_SS, 0);
                    g_led_clolr = 0x0000FF; // blue
                    g_motor_status = 1;
                } else {
                    gpio_set_level(MOTOR_OUTPUT_SS, 1);
                    g_led_clolr = 0xFF0000; // red
                    g_motor_status = 0;
                }
            }
        }
    }
}

static int motor_gpio_init(void)
{
    ESP_LOGI(TAG, "Motor GPIO initialized");

    gpio_config_t motot_io_output = {};
    motot_io_output.intr_type = GPIO_INTR_DISABLE;
    motot_io_output.mode = GPIO_MODE_OUTPUT;
    motot_io_output.pin_bit_mask = MOTOR_OUTPUT_PIN_SEL;
    motot_io_output.pull_down_en = 0;
    motot_io_output.pull_up_en = 1;
    gpio_config(&motot_io_output);

    gpio_config_t motot_io_input = {};
    motot_io_input.intr_type = GPIO_INTR_NEGEDGE;
    motot_io_input.pin_bit_mask = MOTOR_INPUT_PIN_SEL;
    motot_io_input.mode = GPIO_MODE_INPUT;
    motot_io_input.pull_up_en = 1;
    gpio_config(&motot_io_input);

    gpio_set_level(MOTOR_OUTPUT_SS, 1);

    //change gpio interrupt type for one pin
    // gpio_set_intr_type(MOTOR_INPUT_FG, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(MOTOR_INPUT_LD, gpio_isr_handler, (void*) MOTOR_INPUT_LD);
    gpio_isr_handler_add(MOTOR_INPUT_FG, gpio_isr_handler, (void*) MOTOR_INPUT_FG);
    gpio_isr_handler_add(MOTOR_INPUT_SWITCH, gpio_isr_handler, (void*) MOTOR_INPUT_SWITCH);

    //remove isr handler for gpio number.
    // gpio_isr_handler_remove(MOTOR_INPUT_FG);

    printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());

    return 0;
}

// #define LEDC_TIMER              LEDC_TIMER_1
// #define LEDC_MODE               LEDC_LOW_SPEED_MODE
// #define LEDC_OUTPUT_IO          (3) // Define the output GPIO
// #define LEDC_CHANNEL            LEDC_CHANNEL_1
// #define LEDC_DUTY_RES           LEDC_TIMER_12_BIT // Set duty resolution to 13 bits
// #define LEDC_FREQUENCY          (10000) // Frequency in

// static void laser_init(void)
// {
//     // Prepare and then apply the LEDC PWM timer configuration
//     ledc_timer_config_t ledc_timer = {
//         .speed_mode       = LEDC_MODE,
//         .timer_num        = LEDC_TIMER,
//         .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
//         .duty_resolution  = LEDC_DUTY_RES,
//         .clk_cfg          = LEDC_AUTO_CLK
//     };
//     ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

//     // Prepare and then apply the LEDC PWM channel configuration
//     ledc_channel_config_t ledc_channel = {
//         .speed_mode     = LEDC_MODE,
//         .timer_sel      = LEDC_TIMER,
//         .channel        = LEDC_CHANNEL,
//         .intr_type      = LEDC_INTR_DISABLE,
//         .gpio_num       = LEDC_OUTPUT_IO,
//         .duty           = 0, // Set duty to 0%
//         .hpoint         = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

//     ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
//     ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
// }


void user_projection_task(void *pvParameters)
{
    g_gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    motor_gpio_init();
    // laser_init();
    xTaskCreate(projection_gpio_interrupt_task, "projection_gpio_interrupt_task", 2048, NULL, 10, NULL);
    uint32_t duty = 2048;
    while (1) {
        // ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
        // ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
        // duty += 48;
        // if (duty >= 4096) {
        //     duty = 2048;
        // }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
