#pragma once

#include <stdbool.h>
#include "zigbee_main.h"

/**
 * @brief Enum to define dimming modes.
 */
typedef enum {
    DIMMING_MODE_BASIC,
    DIMMING_MODE_ADVANCED
} dimming_mode_t;

/**
 * @brief Basic structure to hold fade parameters for a single light.
 */
typedef struct {
    esp_zb_ieee_addr_t address;
    uint8_t id;
    bool use_gamma;
    bool use_lut;
    bool with_onoff;
    double offset;
    // We store references or copies of the config as needed.
    // ... If you want each device to have unique times, store them here.
    // Otherwise, you'll reference the global g_light_config.
    // Example:
    uint16_t on_time;
    uint16_t off_time;
    uint8_t level_min;
    uint8_t level_max;
    uint16_t transition_time;
    double gamma_value;
    dimming_mode_t dimming_mode;
    double smooth;
    // ...
    TaskHandle_t task_handle;
} light_fade_t;

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
