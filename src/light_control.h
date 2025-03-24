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

typedef enum {
    DIMMING_STRATEGY_MOVE_TO_LEVEL_WITH_OFF_OFF,
    DIMMING_STRATEGY_MOVE_TO_LEVEL,
    DIMMING_STRATEGY_LEVEL_MOVE_WITH_ON_OFF,
    DIMMING_STRATEGY_LEVEL_MOVE
} dimming_strategy_t;


typedef enum {
    GAMMA_MODE_LINEAR,
    GAMMA_MODE_EXPONENTIAL,
    GAMMA_MODE_POLYNOMIAL
} gamma_mode_t;


typedef struct {
    double time;
    int brightness;
} brightness_step_t;

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
    double on_time;
    double off_time;
    uint8_t level_min;
    uint8_t level_max;
    double transition_time;
    double gamma_pow_value;
    double gamma_pow_scale;
    double gamma_pol_a;
    double gamma_pol_b;
    double gamma_pol_c;
    gamma_mode_t gamma_mode;
    dimming_mode_t dimming_mode;
    dimming_strategy_t dimming_strategy;
    double smooth;
    TaskHandle_t task_handle;
    brightness_step_t * step_table;
    size_t step_table_size;
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
