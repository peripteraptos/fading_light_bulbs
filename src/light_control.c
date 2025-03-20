#include "light_control.h"
#include <string.h>
#include <math.h>
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_config.h"
#include "zigbee_main.h"

static const char *TAG = "LIGHT_CONTROL";

/* For demonstration, define two lights. */
static light_fade_t lamp1_fade, lamp2_fade;

/* Global flag to pause/resume fade. In actual projects, you might
 * keep one flag per lamp or store in the structure. */
static bool g_fade_paused = false;

/* Some simplified ZCL commands. You can unify them if you like. */
void level_move(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_level_move_cmd_t cmd_move = {0};
    cmd_move.zcl_basic_cmd.src_endpoint = 1;
    cmd_move.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move.move_mode = mode;
    cmd_move.rate = rate;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_cmd_req(&cmd_move);
    esp_zb_lock_release();
}

void move_to_level_with_onoff(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_move_to_level_cmd_t cmd_move_to = {0};
    cmd_move_to.zcl_basic_cmd.src_endpoint = 1;
    cmd_move_to.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move_to.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move_to.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move_to.level = level;
    cmd_move_to.transition_time = transition_time;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&cmd_move_to);
    esp_zb_lock_release();
}

/**
 * @brief A gamma-based fade task (one approach).
 * You can also implement a simpler linear approach.
 */
static void light_fade_gamma_rtos_task(void *pvParameters)
{
    light_fade_t *light_fade = (light_fade_t *)pvParameters;

    double current_level = g_light_config.levelMin;
    bool going_up = true;
    
    // Convert times from the config (in 100ms units) to milliseconds:
    // This depends on how you interpret the config. 
    uint32_t on_ms         = g_light_config.onTime * 100;
    uint32_t off_ms        = g_light_config.offTime * 100;
    uint32_t transition_ms = g_light_config.transitionTime * 100;
    double   gamma         = g_light_config.gammaVal;

    // If you want each lamp to start at a different offset:
    vTaskDelay((((2 * g_light_config.transitionTime) 
                 + g_light_config.onTime + g_light_config.offTime) 
                * 100 * /* offset */ 0) / portTICK_PERIOD_MS);

    while (1) {
        if (!g_fade_paused) {
            uint8_t target_brightness = going_up ? g_light_config.levelMax : g_light_config.levelMin;
            ESP_LOGI(TAG, "Changing brightness -> %d for lamp at %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     target_brightness,
                     light_fade->address[0], light_fade->address[1],
                     light_fade->address[2], light_fade->address[3],
                     light_fade->address[4], light_fade->address[5],
                     light_fade->address[6], light_fade->address[7]);

            move_to_level_with_onoff(target_brightness, g_light_config.transitionTime, light_fade->address);

            // If going up to max brightness, wait onTime, else wait offTime
            if (going_up) {
                vTaskDelay(on_ms / portTICK_PERIOD_MS);
            } else {
                vTaskDelay(off_ms / portTICK_PERIOD_MS);
            }
            // Wait the transition time
            vTaskDelay(transition_ms / portTICK_PERIOD_MS);

            // Toggle
            going_up = !going_up;
        } else {
            // If paused, just delay a bit
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void lights_init(void)
{
    /* Stop any running tasks first, just to be safe. */
    lights_stop();

    /* You’d set each lamp’s address, offset, etc. 
       For demonstration, we do a static address. */

    // Lamp 1
    memcpy(lamp1_fade.address, 
           (uint8_t[]){0x5d, 0x23, 0x38, 0xfe, 0xff, 0xf8, 0xe2, 0x44}, 
           sizeof(esp_zb_ieee_addr_t));
    lamp1_fade.useGamma = true;

    // Lamp 2
    memcpy(lamp2_fade.address, 
           (uint8_t[]){0xd0, 0x46, 0x3a, 0xfe, 0xff, 0xf8, 0xe2, 0x44}, 
           sizeof(esp_zb_ieee_addr_t));
    lamp2_fade.useGamma = true;

    // Move both to some safe level first
    move_to_level_with_onoff(g_light_config.levelMin, 0, lamp1_fade.address);
    move_to_level_with_onoff(g_light_config.levelMin, 0, lamp2_fade.address);

    // Start tasks
    xTaskCreate(light_fade_gamma_rtos_task, 
                "lamp1_fade_task", 
                4096, 
                &lamp1_fade, 
                5, 
                &lamp1_fade.task_handle);

    xTaskCreate(light_fade_gamma_rtos_task, 
                "lamp2_fade_task", 
                4096, 
                &lamp2_fade, 
                5, 
                &lamp2_fade.task_handle);
}

void lights_stop(void)
{
    // Stop lamp1
    if (lamp1_fade.task_handle != NULL) {
        vTaskDelete(lamp1_fade.task_handle);
        lamp1_fade.task_handle = NULL;
    }

    // Stop lamp2
    if (lamp2_fade.task_handle != NULL) {
        vTaskDelete(lamp2_fade.task_handle);
        lamp2_fade.task_handle = NULL;
    }

    // Optionally set them to a default level
    // ...
}
