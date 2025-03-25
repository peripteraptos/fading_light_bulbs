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

int cmd_get_config(int argc, char **argv)
{
    printf("Current light configuration:\n");
    printf("VALUE offset_1 %.2f\n", g_light_config.offset_1);
    printf("VALUE offset_2 %.2f\n", g_light_config.offset_2);
    printf("VALUE level_min %u\n", g_light_config.level_min);
    printf("VALUE level_max %u\n", g_light_config.level_max);
    printf("VALUE on_time %.2f\n", g_light_config.on_time);
    printf("VALUE off_time %.2f\n", g_light_config.off_time);
    printf("VALUE transition_time %.2f\n", g_light_config.transition_time);
    printf("VALUE dimming_mode %u\n", g_light_config.dimming_mode);
    printf("VALUE dimming_strategy %u\n", g_light_config.dimming_strategy);
    printf("VALUE bezier_p1 %.2f\n", g_light_config.bezier_p1);
    printf("VALUE bezier_p2 %.2f\n", g_light_config.bezier_p2);
    printf("VALUE gamma_mode %u\n", g_light_config.gamma_mode);
    printf("VALUE smooth %.2f\n", g_light_config.smooth);
    printf("VALUE bezier_points");
        for (size_t i = 0; i < g_num_bezier_points; ++i) {
        printf(" %.2f %.2f", g_bezier_points[i].x, 1-g_bezier_points[i].y);
        }
    printf("\n");
    return 0;
}

static int cmd_set_config(int argc, char **argv)
{
    if (argc < 3)
    {
        ESP_LOGW(TAG, "Usage: set <param> <value>");
        return 1;
    }

    const char *param = argv[1];
    double value = atof(argv[2]);

    if (strcmp(param, "offset_1") == 0)
    {
        g_light_config.offset_1 = value;
    }
    else if (strcmp(param, "offset_2") == 0)
    {
        g_light_config.offset_2 = value;
    }
    else if (strcmp(param, "level_min") == 0)
    {
        g_light_config.level_min = (uint8_t)value;
    }
    else if (strcmp(param, "level_max") == 0)
    {
        g_light_config.level_max = (uint8_t)value;
    }
    else if (strcmp(param, "on_time") == 0)
    {
        g_light_config.on_time = value;
    }
    else if (strcmp(param, "off_time") == 0)
    {
        g_light_config.off_time = value;
    }
    else if (strcmp(param, "transition_time") == 0)
    {
        g_light_config.transition_time = value;
    }
    else if (strcmp(param, "smooth") == 0)
    {
        g_light_config.smooth = value;
    }
    else if (strcmp(param, "dimming_mode") == 0)
    {
        g_light_config.dimming_mode = (dimming_mode_t)value;
    }
    else if (strcmp(param, "gamma_mode") == 0)
    {
        g_light_config.gamma_mode = (uint8_t)value;
    }
    else if (strcmp(param, "dimming_strategy") == 0)
    {
        g_light_config.dimming_strategy = (uint8_t)value;
    }
    else if (strcmp(param, "bezier_points") == 0)
    {
        if (argc < 4 || argc % 2 != 0)
        {
            ESP_LOGW(TAG, "Usage: set bezier_points <x1> <y1> <x2> <y2> ...");
            return 1;
        }

        size_t num_points = (argc - 2) / 2;
        if (num_points > 200)
        {
            ESP_LOGW(TAG, "Too many bezier points provided. Maximum is %zu.", 200);
            return 1;
        }

        for (size_t i = 0; i < num_points; ++i)
        {
            g_bezier_points[i].x = atof(argv[2 + i * 2]);
            g_bezier_points[i].y = 1 - atof(argv[3 + i * 2]);
        }

        g_num_bezier_points = num_points;
    }
    else
    {
        ESP_LOGW(TAG, "Unknown parameter: %s", param);
        return 1;
    }

    ESP_LOGI(TAG, "Updated %s to %f", param, value);

    // Re-initialize the lights to apply new settings
    lights_init();
    return 0;
}

static int cmd_start_calibration(int argc, char **argv)
{
    ESP_LOGI(TAG, "Starting calibration task...");
    start_calibration();
    return 0;
}

static int cmd_save_config(int argc, char **argv)
{
    ESP_LOGI(TAG, "Saving current configuration to non-volatile storage...");
    save_current_light_config_to_nvs();
    return 0;
}

static int cmd_reset_config(int argc, char **argv)
{
    ESP_LOGI(TAG, "Resetting configuration to default values...");
    reset_light_config_to_default();
    lights_init(); // Re-initialize the lights with default settings
    return 0;
}

static int cmd_reload_config(int argc, char **argv)
{
    ESP_LOGI(TAG, "Reloading configuration from non-volatile storage...");
    load_light_config_from_nvs();
    lights_init(); // Re-initialize the lights with reloaded settings
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

    // "save" command
    const esp_console_cmd_t save_cmd = {
        .command = "save_light_config",
        .help = "Save current configuration to non-volatile storage",
        .hint = NULL,
        .func = &cmd_save_config,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&save_cmd));

    // "reset" command
    const esp_console_cmd_t reset_cmd = {
        .command = "reset_light_config",
        .help = "Reset configuration to default values",
        .hint = NULL,
        .func = &cmd_reset_config,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reset_cmd));

    // "reload" command
    const esp_console_cmd_t reload_cmd = {
        .command = "reload_light_config",
        .help = "Reload configuration from non-volatile storage",
        .hint = NULL,
        .func = &cmd_reload_config,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reload_cmd));
}
