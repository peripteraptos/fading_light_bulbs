#pragma once

#include <stdint.h>

typedef enum {
    DIMMING_MODE_LINEAR,
    DIMMING_MODE_EXPONENTIAL,
    DIMMING_MODE_LOGARITHMIC
} dimmingMode_t;

/**
 * @brief Structure to hold all “light” configuration parameters.
 * You can expand or rename these fields as needed.
 */
typedef struct {
    uint16_t offset1;
    uint16_t offset2;
    uint8_t  levelMin;
    uint8_t  levelMax;
    uint16_t onTime;          // in 100ms units, or however you prefer
    uint16_t offTime;         // in 100ms units
    uint16_t transitionTime;  // in 100ms units
    dimmingMode_t dimmingMode;
    double   gammaVal;
} light_config_t;

/**
 * @brief Global instance of configuration,
 * accessible from anywhere (or prefer passing references).
 */
extern light_config_t g_light_config;