#include "app_config.h"
#include "string.h"

/*
 * For demonstration, we define example lamp addresses here.
 * Replace with your actual lamp addresses or discover them dynamically.
 */
esp_zb_ieee_addr_t lamp1_long_address = {0x5d, 0x23, 0x38, 0xfe, 0xff, 0xf8, 0xe2, 0x44};
esp_zb_ieee_addr_t lamp2_long_address = {0xd0, 0x46, 0x3a, 0xfe, 0xff, 0xf8, 0xe2, 0x44};

uint8_t g_gamma_lookup_table[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
    1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,
    4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,
    9,   9,  10,  10,  11,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,
   16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  23,  23,  24,  24,
   25,  26,  26,  27,  28,  28,  29,  30,  30,  31,  32,  32,  33,  34,  35,  35,
   36,  37,  38,  38,  39,  40,  41,  42,  42,  43,  44,  45,  46,  47,  47,  48,
   49,  50,  51,  52,  53,  54,  55,  56,  56,  57,  58,  59,  60,  61,  62,  63,
   64,  65,  66,  67,  68,  69,  70,  71,  73,  74,  75,  76,  77,  78,  79,  80,
   81,  82,  84,  85,  86,  87,  88,  89,  91,  92,  93,  94,  95,  97,  98,  99,
  100, 102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120,
  121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143,
  145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166, 168,
  170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195,
  197, 199, 200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224,
  226, 228, 230, 232, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
 };

light_config_t g_light_config;
/* Provide default values for your global config. */
light_config_t g_light_config_default = {
    .offset_1 = 0,
    .offset_2 = 0.5,
    .level_min = 5,
    .level_max = 254,
    .on_time = 0,
    .off_time = 0,
    .transition_time = 10.0,
    .bezier_p1 = 0.5,
    .bezier_p2 = 0.75,
    .gamma_mode = GAMMA_MODE_LINEAR,
    .smooth = 0,
    .dimming_mode = DIMMING_MODE_ADVANCED,
    .dimming_strategy = DIMMING_STRATEGY_MOVE_TO_LEVEL
};


uint8_t g_inverseLUT[256];

bezier_point_t g_bezier_points[256] = {
    {0.0, 0.0},
    {0.25, 0.1},
    {0.5, 0.5},
    {0.75, 0.9},
    {1.0, 1.0}
};


size_t g_num_bezier_points = 5;
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