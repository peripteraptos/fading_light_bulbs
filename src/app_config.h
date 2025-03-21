#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <light_control.h>
#include "nvs_flash.h"
#include "nvs.h"


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
    double smooth;
} light_config_t;

extern light_config_t g_light_config;
extern light_config_t g_light_config_default;


#define NVS_NAMESPACE "storage"
#define NVS_KEY "light_config"

// Function to load the configuration from flash
esp_err_t load_light_config_from_nvs();

// Function to save the configuration to flash
esp_err_t save_light_config_to_nvs(const light_config_t *config);

// Function to reset the configuration to defaults
esp_err_t reset_light_config_to_default();

esp_err_t save_current_light_config_to_nvs();