#include "app_config.h"
#include "string.h"

light_config_t g_light_config;

/* Provide default values for your global config. */
light_config_t g_light_config_default = {
    .offset_1 = 0,
    .offset_2 = 0.5,
    .level_min = 5,
    .level_max = 254,
    .on_time = 0,
    .off_time = 0,
    .transition_time = 20,
    .use_gamma = true,
    .use_lut = false,
    .gamma_value = 2.2,
    .smooth = 0,
    .gamma_lookup_table = {
        0.000000,
        0.000006,
        0.000038,
        0.000110,
        0.000232,
        0.000414,
        0.000666,
        0.000994,
        0.001406,
        0.001910,
        0.002512,
        0.003218,
        0.004035,
        0.004969,
        0.006025,
        0.007208,
        0.008525,
        0.009981,
        0.011580,
        0.013328,
        0.015229,
        0.017289,
        0.019512,
        0.021902,
        0.024465,
        0.027205,
        0.030125,
        0.033231,
        0.036527,
        0.040016,
        0.043703,
        0.047593,
        0.051688,
        0.055993,
        0.060513,
        0.065249,
        0.070208,
        0.075392,
        0.080805,
        0.086451,
        0.092333,
        0.098455,
        0.104821,
        0.111434,
        0.118298,
        0.125416,
        0.132792,
        0.140428,
        0.148329,
        0.156498,
        0.164938,
        0.173653,
        0.182645,
        0.191919,
        0.201476,
        0.211321,
        0.221457,
        0.231886,
        0.242612,
        0.253639,
        0.264968,
        0.276603,
        0.288548,
        0.300805,
        0.313378,
        0.326268,
        0.339480,
        0.353016,
        0.366879,
        0.381073,
        0.395599,
        0.410461,
        0.425662,
        0.441204,
        0.457091,
        0.473325,
        0.489909,
        0.506846,
        0.524138,
        0.541789,
        0.559801,
        0.578177,
        0.596920,
        0.616032,
        0.635515,
        0.655374,
        0.675610,
        0.696226,
        0.717224,
        0.738608,
        0.760380,
        0.782542,
        0.805097,
        0.828048,
        0.851398,
        0.875148,
        0.899301,
        0.923861,
        0.948829,
        0.974208,
        1.000000,
    },
    .dimming_mode = DIMMING_MODE_ADVANCED};

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