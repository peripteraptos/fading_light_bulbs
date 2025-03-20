#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "cmd_system.h"
#include "argtable3/argtable3.h"
#include "console_cmd.h"
#include "app_config.h"
#include "calibration.h"
#include "light_control.h"

static const char *TAG = "CONSOLE_CMD";

static int cmd_get_config(int argc, char **argv)
{
    printf("Current light configuration:\n");
    printf("VALUE offset_1 %u\n", g_light_config.offset1);
    printf("VALUE offset_2 %u\n", g_light_config.offset2);
    printf("VALUE level_min %u\n", g_light_config.levelMin);
    printf("VALUE level_max %u\n", g_light_config.levelMax);
    printf("VALUE on_time %u\n", g_light_config.onTime);
    printf("VALUE off_time %u\n", g_light_config.offTime);
    printf("VALUE transition_time %u\n", g_light_config.transitionTime);
    printf("VALUE gamma %.2f\n", g_light_config.gammaVal * 100);
    return 0;
}

static int cmd_set_config(int argc, char **argv)
{
    if (argc < 3) {
        ESP_LOGW(TAG, "Usage: set <param> <value>");
        return 1;
    }

    const char *param = argv[1];
    double value = atof(argv[2]);

    if (strcmp(param, "offset1") == 0) {
        g_light_config.offset1 = (uint16_t)value;
    } else if (strcmp(param, "offset2") == 0) {
        g_light_config.offset2 = (uint16_t)value;
    } else if (strcmp(param, "levelMin") == 0) {
        g_light_config.levelMin = (uint8_t)value;
    } else if (strcmp(param, "levelMax") == 0) {
        g_light_config.levelMax = (uint8_t)value;
    } else if (strcmp(param, "onTime") == 0) {
        g_light_config.onTime = (uint16_t)value;
    } else if (strcmp(param, "offTime") == 0) {
        g_light_config.offTime = (uint16_t)value;
    } else if (strcmp(param, "transitionTime") == 0) {
        g_light_config.transitionTime = (uint16_t)value;
    } else if (strcmp(param, "gamma") == 0) {
        g_light_config.gammaVal = value;
    } else {
        ESP_LOGW(TAG, "Unknown parameter: %s", param);
        return 1;
    }

    ESP_LOGI(TAG, "Updated %s to %f", param, value);

    // Re-initialize the lights to apply new settings
    // lights_init();
    return 0;
}

static int cmd_start_calibration(int argc, char **argv)
{
    ESP_LOGI(TAG, "Starting calibration task...");
    // start_calibration_task();
    return 0;
}

void register_console_commands(void)
{
    register_system();

    // "get" command
    const esp_console_cmd_t get_cmd = {
        .command = "get",
        .help = "Get current configuration values",
        .hint = NULL,
        .func = &cmd_get_config,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&get_cmd));

    // "set" command
    const esp_console_cmd_t set_cmd = {
        .command = "set",
        .help = "Set parameter. Usage: set <offset1|offset2|levelMin|...> <value>",
        .hint = NULL,
        .func = &cmd_set_config,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));

    // "start_calibration" command
    const esp_console_cmd_t calib_cmd = {
        .command = "start_calibration",
        .help = "Start the calibration task",
        .hint = NULL,
        .func = &cmd_start_calibration,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&calib_cmd));
}
