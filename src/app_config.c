#include "app_config.h"
#include "string.h"

/*
 * For demonstration, we define example lamp addresses here.
 * Replace with your actual lamp addresses or discover them dynamically.
 */
esp_zb_ieee_addr_t lamp1_long_address = {0x5d, 0x23, 0x38, 0xfe, 0xff, 0xf8, 0xe2, 0x44};
esp_zb_ieee_addr_t lamp2_long_address = {0xd0, 0x46, 0x3a, 0xfe, 0xff, 0xf8, 0xe2, 0x44};

light_config_t g_light_config;
/* Provide default values for your global config. */
light_config_t g_light_config_default = {
    .offset_1 = 0,
    .offset_2 = 0.5,
    .level_min = 0,
    .level_max = 255,
    .on_time = 0.0f,
    .off_time = 0.0f,   
    .transition_time = 10.0,
    .gamma_pow_scale = 1.1,
    .gamma_pow_value = 2.2,
    .gamma_log_value = 5,
    .gamma_mode = GAMMA_MODE_LINEAR,
    .dimming_mode = DIMMING_MODE_ADVANCED,
    .dimming_strategy = DIMMING_STRATEGY_MOVE_TO_LEVEL,
    .step_table_size = 30,
};

// Fitted in normalized space: yScaled = 0.557 * xScaled^(1.018)
// Fitted polynomial: y = 0.0639 + -0.2980*x + 1.2331*x^2
// Predicted Y for x=100 is 12301.364258

// Function to load the configuration from flash
esp_err_t load_light_config_from_nvs()
{
    nvs_handle_t nvs_handle;
    size_t required_size = sizeof(light_config_t);
    memcpy(&g_light_config, &g_light_config_default, required_size);
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK)
    {
        if (nvs_get_blob(nvs_handle, NVS_KEY, &g_light_config, &required_size) != ESP_OK)
        {
            // If reading failed, we don't change 'config', it remains default
        }
        nvs_close(nvs_handle);
    }

    // return g_light_config;
    return ESP_OK;
}

// Function to save the configuration to flash
esp_err_t save_light_config_to_nvs(const light_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    if ((err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle)) == ESP_OK)
    {
        if ((err = nvs_set_blob(nvs_handle, NVS_KEY, config, sizeof(light_config_t))) == ESP_OK)
        {
            err = nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    }

    return err;
}

// Function to save the current configuration to flash
esp_err_t save_current_light_config_to_nvs()
{
    return save_light_config_to_nvs(&g_light_config);
}

// Function to reset the configuration to defaults
esp_err_t reset_light_config_to_default()
{
    save_light_config_to_nvs(&g_light_config_default);
    return load_light_config_from_nvs();
}