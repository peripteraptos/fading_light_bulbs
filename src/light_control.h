#pragma once

#include <stdbool.h>
#include "zigbee_main.h"
#include <math.h>
#include <stdint.h>

#define MAX_SEGMENTS 255

/**
 * @brief Enum to define dimming modes.
 */
typedef enum {
    DIMMING_MODE_BASIC,
    DIMMING_MODE_ADVANCED
} dimming_mode_t;

typedef enum {
    DIMMING_STRATEGY_MOVE_TO_LEVEL_WITH_OFF_OFF,
    DIMMING_STRATEGY_MOVE_TO_LEVEL,
    DIMMING_STRATEGY_LEVEL_MOVE_WITH_ON_OFF,
    DIMMING_STRATEGY_LEVEL_MOVE
} dimming_strategy_t;

typedef struct {
    float fraction_of_fade;   // e.g. 0.0, 0.25, 0.5, 0.75, 1.0
    uint8_t level;           // the 8-bit Zigbee level for that fraction
} fade_segment_t;


typedef enum {
    GAMMA_MODE_LINEAR,
    GAMMA_MODE_EXPONENTIAL,
    GAMMA_MODE_LOGARITHMIC
} gamma_mode_t;

typedef enum {
    CURVE_TYPE_LINEAR,
    CURVE_TYPE_SINE,
    CURVE_TYPE_QUADRATIC,
    CURVE_TYPE_CUBIC,
    CURVE_TYPE_QUARTIC
} curve_type_t;

/**
 * @brief Basic structure to hold fade parameters for a single light.
 */
typedef struct {
    esp_zb_ieee_addr_t address;
    uint8_t id;
    double offset;
    TaskHandle_t task_handle;
    fade_segment_t * fade_table;
} light_fade_t;

/**
 * @brief Stop and delete fade tasks for all lights.
 */
void lights_stop(void);

void lights_init(void);

/**
 * @brief Sends a move-to-level with on/off command (common usage).
 */
void move_to_level_with_onoff(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address);

/**
 * @brief Sends a move-to-level command (common usage).
 */
void move_to_level(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address);

/**
 * @brief Low-level function (example) to do a “level move” without on/off.
 */
void level_move(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address);
