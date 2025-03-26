#include "light_control.h"
#include <string.h>
#include <math.h>
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_config.h"
#include "zigbee_main.h"
#include <math.h>
#include "stdlib.h"
#include "light_helper.h"

static const char *TAG = "LIGHT_CONTROL";

/* For demonstration, define two lights. */
static light_fade_t lamp1_fade, lamp2_fade;



double log_transform(double x, double B)
{
    // Safeguard: If B <= 0, the transform doesn't make sense as intended.
    if (B <= 0.0)
    {
        // Return x unchanged or handle as an error
        return x;
    }

    // Safeguard: If x < 0, clamp to 0; if x > 1, clamp to 1.
    if (x < 0.0)
        x = 0.0;
    if (x > 1.0)
        x = 1.0;

    // Compute denominator
    double denom = log(1.0 + B);
    // Compute numerator
    double numerator = log(1.0 + B * x);

    // Because x is in [0,1], numerator <= denom. So out in [0,1].
    return numerator / denom;
}




static void build_gamma_fade_table(int segments, light_fade_t *light_fade, fade_segment_t *fade_table)
{

    for (int i = 0; i < segments; i++)
    {
        // Calculate fraction based on linear index
        float fraction = (float)i / (float)(segments - 1);
        
        if(g_light_config.curve_type == CURVE_TYPE_SINE){
            // Use sinusoidal function to calculate fraction
            fraction = 0.5f * (1.0f - cosf(fraction * (float)M_PI));
        }

        uint8_t abs_max_level = 255;

        uint8_t min_level = g_light_config.level_min;
        uint8_t max_level = g_light_config.level_max;

        fraction *= abs_max_level / (max_level - min_level);
        fraction += g_light_config.level_min / abs_max_level;

        // Apply gamma correction
        float corrected = fraction;

        if (g_light_config.gamma_mode == GAMMA_MODE_EXPONENTIAL)
        {
            float scale = g_light_config.gamma_pow_scale;
            float value = g_light_config.gamma_pow_value;
            if (value == 0)
                value = 1;

            corrected = scale * pow(fraction, 1 / value) - scale + 1; // 0..1
            // corrected = pow(fraction, 1/value);
        }
        else if (g_light_config.gamma_mode == GAMMA_MODE_LOGARITHMIC)
        {
            corrected = log_transform(fraction, g_light_config.gamma_log_value);
        }

        // Then map to 8-bit level [min_level..max_level]
        float level_f = min_level + (max_level - min_level) * corrected;
        if (level_f > max_level)
            level_f = max_level; // clamp
        if (level_f < min_level)
            level_f = min_level;

        fade_table[i].fraction_of_fade = (float)i / (float)(segments - 1);
        fade_table[i].level = (uint8_t)roundf(level_f);
    }
}

static void do_piecewise_fade_blocking(light_fade_t *light_fade,
                                       fade_segment_t *segments,
                                       int segment_count,
                                       float total_fade_s)
{
    for (int dir = 0; dir < 2; dir++) // Two directions: up and down
    {
        int start = (dir == 0) ? 0 : segment_count - 1;
        int end = (dir == 0) ? segment_count - 1 : 0;
        int step = (dir == 0) ? 1 : -1;

        // Iterate through the segments based on direction
        for (int i = start; (dir == 0 ? i < end : i > end); i += step)
        {
            float fraction_start = segments[i].fraction_of_fade;
            float fraction_end = segments[i + step].fraction_of_fade;

            // How long this piece of the fade should take
            float seg_duration_s = fabsf(fraction_end - fraction_start) * total_fade_s;
            // The final level for the next segment
            uint8_t target_level = segments[i + step].level;

            // Zigbee transition_time is in 1/10ths of a second
            uint16_t transition_time_1_10s = (uint16_t)(seg_duration_s * 10.0f);

            ESP_LOGD("FADE", "Segment %d->%d: fraction %.2f->%.2f, level %d->%d, seg_duration=%.2fs",
                     i, i + step,
                     fraction_start, fraction_end,
                     segments[i].level, target_level,
                     seg_duration_s);

            ESP_LOGI(TAG, "Setting Lamp%d to %d within %dms",
                     light_fade->id, target_level, (int)(seg_duration_s * 1000));

            // Send the command
            move_to_level(target_level, transition_time_1_10s, light_fade->address);

            // Block the task until this segment is done
            vTaskDelay(pdMS_TO_TICKS((int)(seg_duration_s * 1000)));
        }

        // Wait at the end of the segment traversal
        float wait_time = (dir == 0) ? g_light_config.on_time : g_light_config.off_time;
        wait_time = wait_time * g_light_config.transition_time;
        ESP_LOGI(TAG, "Lamp%d waiting for %.2fs", light_fade->id, wait_time);

        vTaskDelay(pdMS_TO_TICKS((int)(wait_time * 1000)));
    }
}

static void light_fade_rtos_task(void *pvParameters)
{

    light_fade_t *light_fade = (light_fade_t *)pvParameters;

    // You can tweak gamma, # of segments, etc.
    static fade_segment_t fade_table[MAX_SEGMENTS];

    int segments = g_light_config.step_table_size;
    build_gamma_fade_table(segments, light_fade, fade_table);

    // Calculate the cycle time using transition_time, on_time and off_time
    float total_transition_time_s = g_light_config.transition_time * 2;
    // Calculate on_time and off_time as fractions of transition_time
    float on_time_s = g_light_config.on_time * g_light_config.transition_time;
    float off_time_s = g_light_config.off_time * g_light_config.transition_time;

    // Calculate the cycle time using transition_time, on_time, and off_time
    float cycle_time_s = total_transition_time_s + on_time_s + off_time_s;
    vTaskDelay(light_fade->offset * cycle_time_s * 1000 / portTICK_PERIOD_MS);

    while (1)
    {
        do_piecewise_fade_blocking(light_fade, fade_table, segments, g_light_config.transition_time);

        ESP_LOGI("FADE", "Gamma fade complete");
    }
}

void lights_init(void)
{
    /* Stop any running tasks first, just to be safe. */
    lights_stop();

    /* You’d set each lamp’s address, offset, etc.
       For demonstration, we do a static address. */

    // Lamp 1
    memcpy(lamp1_fade.address,
           (uint8_t[]){0x5d, 0x23, 0x38, 0xfe, 0xff, 0xf8, 0xe2, 0x44},
           sizeof(esp_zb_ieee_addr_t));

    lamp1_fade.offset = g_light_config.offset_1;
    lamp1_fade.id = 1;

    // Lamp 2
    memcpy(&lamp2_fade, &lamp1_fade, sizeof(light_fade_t));
    memcpy(lamp2_fade.address,
           (uint8_t[]){0xd0, 0x46, 0x3a, 0xfe, 0xff, 0xf8, 0xe2, 0x44},
           sizeof(esp_zb_ieee_addr_t));
    lamp2_fade.offset = g_light_config.offset_2;
    lamp2_fade.id = 2;

    // Move both to some safe level first
    move_to_level_with_onoff(10, 0, lamp1_fade.address);
    move_to_level_with_onoff(10, 0, lamp2_fade.address);

    // Start tasks
    xTaskCreate(light_fade_rtos_task,
                "lamp1_fade_task",
                4096,
                &lamp1_fade,
                4,
                &lamp1_fade.task_handle);

    xTaskCreate(light_fade_rtos_task,
                "lamp2_fade_task",
                4096,
                &lamp2_fade,
                4,
                &lamp2_fade.task_handle);
}

void lights_stop(void)
{
    // Stop lamp1
    if (lamp1_fade.task_handle != NULL)
    {
        vTaskDelete(lamp1_fade.task_handle);
        lamp1_fade.task_handle = NULL;
    }

    // Stop lamp2
    if (lamp2_fade.task_handle != NULL)
    {
        vTaskDelete(lamp2_fade.task_handle);
        lamp2_fade.task_handle = NULL;
    }
}
