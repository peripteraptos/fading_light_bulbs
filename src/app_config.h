#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <light_control.h>


/**
 * @brief Structure to hold all “light” configuration parameters.
 * You can expand or rename these fields as needed.
 */
typedef struct {
    double offset_1;
    double offset_2;
    uint8_t level_min;
    uint8_t level_max;
    double on_time;             // in 100ms units, or however you prefer
    double off_time;            // in 100ms units
    double transition_time;     // in 100ms units
    dimming_mode_t dimming_mode;
    bool use_gamma;
    bool use_lut;
    double gamma_value;
    double gamma_lookup_table[256];
} light_config_t;

/**
 * @brief Global instance of configuration,
 * accessible from anywhere (or prefer passing references).
 */
extern light_config_t g_light_config;