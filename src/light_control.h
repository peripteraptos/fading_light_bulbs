#pragma once

#include <stdbool.h>
#include "zigbee_main.h"

/**
 * @brief Basic structure to hold fade parameters for a single light.
 */
typedef struct {
    esp_zb_ieee_addr_t address;
    bool useGamma;
    // We store references or copies of the config as needed.
    // ... If you want each device to have unique times, store them here.
    // Otherwise, you'll reference the global g_light_config.
    // Example:
    // uint16_t onTime;
    // uint16_t offTime;
    // uint8_t levelMin;
    // uint8_t levelMax;
    // ...
    TaskHandle_t task_handle;
} light_fade_t;

/**
 * @brief Initialize the fade for each light, creating tasks, etc.
 * This might read from g_light_config globally or accept local parameters.
 */
void lights_init(void);

/**
 * @brief Stop and delete fade tasks for all lights.
 */
void lights_stop(void);

/**
 * @brief Sends a move-to-level with on/off command (common usage).
 */
void move_to_level_with_onoff(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address);

/**
 * @brief Low-level function (example) to do a “level move” without on/off.
 */
void level_move(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address);
