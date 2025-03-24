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
#include <limits.h> // for std::numeric_limits<double>::quiet_NaN()

static const char *TAG = "LIGHT_CONTROL";

/* For demonstration, define two lights. */
static light_fade_t lamp1_fade, lamp2_fade;

brightness_step_t lamp1_steps[1024];
brightness_step_t lamp2_steps[1024];

/* Global flag to pause/resume fade. In actual projects, you might
 * keep one flag per lamp or store in the structure. */
static bool g_fade_paused = false;

/* Some simplified ZCL commands. You can unify them if you like. */
void level_move(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_level_move_cmd_t cmd_move = {0};
    cmd_move.zcl_basic_cmd.src_endpoint = 1;
    cmd_move.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move.move_mode = mode;
    cmd_move.rate = rate;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_cmd_req(&cmd_move);
    esp_zb_lock_release();
}

void level_stop(esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_level_stop_cmd_t cmd_stop = {0};
    cmd_stop.zcl_basic_cmd.src_endpoint = 1;
    cmd_stop.zcl_basic_cmd.dst_endpoint = 1;
    cmd_stop.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_stop.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_stop_cmd_req(&cmd_stop);
    esp_zb_lock_release();
}

void move_to_level_with_onoff(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_move_to_level_cmd_t cmd_move_to = {0};
    cmd_move_to.zcl_basic_cmd.src_endpoint = 1;
    cmd_move_to.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move_to.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move_to.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move_to.level = level;
    cmd_move_to.transition_time = transition_time;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(&cmd_move_to);
    esp_zb_lock_release();
}

void move_to_level(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_move_to_level_cmd_t cmd_move_to = {0};
    cmd_move_to.zcl_basic_cmd.src_endpoint = 1;
    cmd_move_to.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move_to.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move_to.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move_to.level = level;
    cmd_move_to.transition_time = transition_time;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_cmd_req(&cmd_move_to);
    esp_zb_lock_release();
}

static double shape_up(double x, double s)
{
    double smoothstep = 3 * x * x - 2 * x * x * x;
    return (1 - s) * x + s * smoothstep;
}

static double shape_down(double x, double s)
{
    return 1 - shape_up(x, s);
}

static double gamma_function(double value)
{
    if (g_light_config.gamma_mode == GAMMA_MODE_EXPONENTIAL)
    {
        double ret = g_light_config.gamma_pow_scale * pow(value, g_light_config.gamma_pow_value);
        if (ret > 1)
            ret = 1;
        if (ret < 0)
            ret = 0;

        return ret;
        // return inverse_exponential_gamma(value);
    }
    if (g_light_config.gamma_mode == GAMMA_MODE_POLYNOMIAL)
    {
        double ret = g_light_config.gamma_pol_a + g_light_config.gamma_pol_b * value + g_light_config.gamma_pol_c * (value * value);
        if (ret > 1)
            ret = 1;
        if (ret < 0)
            ret = 0;

        return ret;
        // return invert_gamma_polynomial(value);
    }

    return value;
}

static double base_brightness(double t, double fadeTime, double on_time, double off_time, double smooth)
{
    double period = 2 * fadeTime + on_time + off_time;
    t = fmod(fmod(t, period) + period, period);
    double upEnd = fadeTime;
    double onEnd = fadeTime + on_time;
    double downEnd = fadeTime + on_time + fadeTime;

    if (t < upEnd)
    {
        double x = t / fadeTime;
        double val = shape_up(x, smooth);
        return gamma_function(val);
    }
    else if (t < onEnd)
    {
        return 1;
    }
    else if (t < downEnd)
    {
        double x = (t - (fadeTime + on_time)) / fadeTime;
        double val = shape_down(x, smooth);
        return gamma_function(val);
    }
    else
    {
        return 0;
    }
}

static double brightness(double t, double fadeTime, double on_time, double off_time, double smooth, double minB, double maxB)
{
    double raw = base_brightness(t, fadeTime, on_time, off_time, smooth);
    return minB + (maxB - minB) * raw;
}

static double get_cycle_time(light_fade_t *lightFade)
{
    return lightFade->transition_time * (2 + lightFade->on_time + lightFade->off_time);
}
static double get_offset_s(light_fade_t *lightFade)
{
    double cycleTime = get_cycle_time(lightFade);
    return cycleTime * lightFade->offset;
}

static double brightness_at_time(double t, light_fade_t *lightFade)
{
    double time_with_offset = t + get_offset_s(lightFade);
    return brightness(time_with_offset, lightFade->transition_time, lightFade->on_time, lightFade->off_time, lightFade->smooth, lightFade->level_min, lightFade->level_max);
}

// =============================
// GENERATE A LIST OF (time, integerBrightness) CROSSINGS
// up to a given maxTime or up to 1 cycle
// =============================
void generateBrightnessSteps(light_fade_t *lightConfig, double min_step_s)
{

    double fadeTime = lightConfig->transition_time;
    double on_time = fadeTime * lightConfig->on_time;
    double off_time = fadeTime * lightConfig->off_time;
    double cycle_s = (2 * fadeTime) + on_time + off_time;

    // We'll scan from t=0 to t=10s with a dt=0.01
    // or we could do just one full cycle => t=0..(2*fadeTime + on_time + off_time)
    double startTime = 0.0;
    double endTime = cycle_s;

    double tOld = startTime;
    double bOld = brightness_at_time(tOld, lightConfig);

    // We can store the integer floor from last step
    int iOld = (int)floor(bOld);

    // Print an initial row if you want the "starting" brightness exactly
    // printf("t = %.5f, brightness = %.2f\n", tOld, bOld);
    lightConfig->step_table_size = 0;

    while (tOld < endTime)
    {
        double tNew = tOld + min_step_s; // Increment time by min_step_ms, converted to seconds
        double bNew = brightness_at_time(tNew, lightConfig);

        // Calculate integer brightness levels
        int iNew = (int)floor(bNew);

        // If we have reached a new integer brightness level
        if (iNew != iOld)
        {
            // printf("t = %.5f, brightness = %.2f\n", tNew, bNew);

            // Add step to the step_table
            if (lightConfig->step_table_size < 1024)
            {
                lightConfig->step_table[lightConfig->step_table_size].time = tNew;
                lightConfig->step_table[lightConfig->step_table_size].brightness = iNew;
                lightConfig->step_table_size++;
            }

            // Update old time and old brightness level
            tOld = tNew;
            iOld = iNew;
        }
        else
        {
            // Move time forward and continue checking
            tOld = tNew;
        }
    }
}

static void light_fade_move_rtos_task(void *pvParameters)
{
    light_fade_t *light_fade = (light_fade_t *)pvParameters;

    while (1)
    {
        if (!g_fade_paused)
        {
            double previous_time = 0.0f;
            double cycle_s = (2 * light_fade->transition_time) + (light_fade->transition_time * light_fade->on_time) + (light_fade->transition_time * light_fade->off_time);
            double t = 0.0f;
            double previous_brightness = 0;
            int rate = 0;
            while (t < cycle_s)
            {
                double current_brightness = brightness_at_time(t, light_fade);

                double time_diff = t - previous_time;
                double brightness_diff = current_brightness - previous_brightness;
                if (time_diff > 0.01){
                     rate = fabs(brightness_diff / time_diff);
                }
                if (rate > 255) rate = 255;

                uint8_t mode = 0;
                if (brightness_diff > 0)
                {
                    mode = 0; // Increasing brightness
                }
                else if (brightness_diff < 0)
                {
                    mode = 1; // Decreasing brightness
                }
                else
                {
                    mode = 0; // No change needed
                    rate = 0; // No rate required
                }

                if (rate > 0)
                {
                    ESP_LOGI(TAG, "Lamp %d: Moving brightness to %f with rate %d (mode %d)",
                        light_fade->id, current_brightness, rate, mode);
                    level_move(mode, (uint8_t)rate, light_fade->address);
                }
                else
                {
                    ESP_LOGI(TAG, "Lamp %d: Moving brightness to %f with rate %d (mode %d)",
                        light_fade->id, current_brightness, rate, mode);
                    level_stop(light_fade->address);
                }

              

                previous_time = t;
                previous_brightness = current_brightness;

                t += 0.25f; // Adjust the time increment as needed
                vTaskDelay(pdMS_TO_TICKS(time_diff * 1000));
            }
        }
        else
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

// /*
//  * More advanced approach, using gamma correction with optional LUT and line fitting,
//  * to achieve the most linear fade possible. The function sends multiple dim commands
//  * to adjust the transition speed in smaller steps.
//  */
// static void light_fade_advanced_rtos_task(void *pvParameters)
// {
//     light_fade_t *light_fade = (light_fade_t *)pvParameters;

//     uint16_t step_ms = 250; // Convert steptime from the config to milliseconds

//     double transition_s = light_fade->transition_time;
//     double on_s = transition_s * light_fade->on_time;
//     double off_s = transition_s * light_fade->off_time;
//     double cycle_s = (2 * transition_s) + on_s + off_s;
//     double offset_s = cycle_s * light_fade->offset;
//     double t = offset_s;

//     while (1)
//     {
//         if (!g_fade_paused)
//         {
//             double current_brightness = brightness(t, transition_s, on_s, off_s, light_fade->smooth,
//                                                    light_fade->level_min, light_fade->level_max);

//             ESP_LOGD(TAG, "Parameters at time %f seconds - fadeTime: %f, on_time: %f, off_time: %f, gamma scale: %f, gamma value: %f, smooth: %f, minB: %d, maxB: %d, target_brightness: %f",
//                      t, transition_s, on_s, off_s, light_fade->gamma_pow_scale, light_fade->gamma_pow_value, light_fade->smooth, light_fade->level_min, light_fade->level_max, current_brightness);

//             uint8_t next_level = (uint8_t)round(current_brightness);

//             ESP_LOGI(TAG, "Setting Lamp%d to %d within %dms",
//                      light_fade->id, next_level, step_ms);

//             if (light_fade->dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL_WITH_OFF_OFF)
//             {
//                 move_to_level_with_onoff(next_level, (uint16_t)step_ms / 100, light_fade->address);
//             }
//             else if (light_fade->dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL)
//             {
//                 move_to_level(next_level, (uint16_t)step_ms / 100, light_fade->address);
//             }

//             // Step time delay to apply brightness changes in intervals
//             vTaskDelay(step_ms / portTICK_PERIOD_MS);

//             // Increment time based on step_ms
//             t += step_ms / 1000.0; // convert ms to seconds
//         }
//         else
//         {
//             // If paused, just delay a bit
//             vTaskDelay(100 / portTICK_PERIOD_MS);
//         }
//     }
// }

// /**
//  * @brief A basic fade task (one approach).
//  */
// static void light_fade_basic_rtos_task(void *pvParameters)
// {
//     light_fade_t *light_fade = (light_fade_t *)pvParameters;

//     // double current_level = g_light_config.level_min;
//     bool going_up = true;

//     // Convert times from the config (in 100ms units) to milliseconds:
//     // This depends on how you interpret the config.
//     uint32_t transition_ms = light_fade->transition_time * 1000;
//     uint32_t on_ms = transition_ms * light_fade->on_time;
//     uint32_t off_ms = transition_ms * light_fade->off_time;
//     uint32_t cycle_ms = (2 * transition_ms) + on_ms + off_ms;
//     uint32_t offset_ms = cycle_ms * light_fade->offset;

//     // If you want each lamp to start at a different offset:
//     vTaskDelay(offset_ms / portTICK_PERIOD_MS);

//     while (1)
//     {
//         if (!g_fade_paused)
//         {
//             uint8_t target_brightness = going_up ? light_fade->level_max : light_fade->level_min;
//             ESP_LOGI(TAG, "Changing brightness -> %d for lamp at %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
//                      target_brightness,
//                      light_fade->address[0], light_fade->address[1],
//                      light_fade->address[2], light_fade->address[3],
//                      light_fade->address[4], light_fade->address[5],
//                      light_fade->address[6], light_fade->address[7]);

//             move_to_level_with_onoff(target_brightness, light_fade->transition_time * 100, light_fade->address);

//             // If going up to max brightness, wait on_time, else wait off_time
//             if (going_up)
//             {
//                 vTaskDelay(on_ms / portTICK_PERIOD_MS);
//             }
//             else
//             {
//                 vTaskDelay(off_ms / portTICK_PERIOD_MS);
//             }
//             // Wait the transition time
//             vTaskDelay(transition_ms / portTICK_PERIOD_MS);

//             // Toggle
//             going_up = !going_up;
//         }
//         else
//         {
//             // If paused, just delay a bit
//             vTaskDelay(100 / portTICK_PERIOD_MS);
//         }
//     }
// }

static void light_fade_rtos_task(void *pvParameters)
{

    light_fade_t *light_fade = (light_fade_t *)pvParameters;
    brightness_step_t *steps = light_fade->step_table;
    size_t steps_n = light_fade->step_table_size;

    while (1)
    {
        double previous_time = 0.0;
        for (int i = 0; i < steps_n; ++i)
        {
            if (steps[i].time > previous_time)
            {
                double delay_time = steps[i].time - previous_time;
                uint8_t brightness_level = (uint8_t)steps[i].brightness;

                if (light_fade->dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL_WITH_OFF_OFF)
                {
                    move_to_level_with_onoff(brightness_level, (uint16_t)(delay_time * 100), light_fade->address);
                }
                else if (light_fade->dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL)
                {
                    move_to_level(brightness_level, (uint16_t)(delay_time * 100), light_fade->address);
                }

                // ESP_LOGI(TAG, "Lamp %d: Setting brightness to %d", light_fade->id, brightness_level);
                ESP_LOGI(TAG, "Setting Lamp%d to %d within %dms",
                         light_fade->id, brightness_level, (uint16_t)(delay_time * 1000));

                vTaskDelay(pdMS_TO_TICKS(delay_time * 1000));

                previous_time = steps[i].time;
            }
        }
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
    lamp1_fade.use_gamma = true;
    lamp1_fade.gamma_pol_a = g_light_config.gamma_pol_a;
    lamp1_fade.gamma_pol_b = g_light_config.gamma_pol_b;
    lamp1_fade.gamma_pol_c = g_light_config.gamma_pol_c;
    lamp1_fade.gamma_pow_scale = g_light_config.gamma_pow_scale;
    lamp1_fade.gamma_pow_value = g_light_config.gamma_pow_value;
    lamp1_fade.gamma_mode = g_light_config.gamma_mode;
    lamp1_fade.on_time = g_light_config.on_time;
    lamp1_fade.off_time = g_light_config.off_time;
    lamp1_fade.offset = g_light_config.offset_1;
    lamp1_fade.dimming_mode = g_light_config.dimming_mode;
    lamp1_fade.dimming_strategy = g_light_config.dimming_strategy;
    lamp1_fade.smooth = g_light_config.smooth;
    lamp1_fade.with_onoff = 0;
    lamp1_fade.level_min = g_light_config.level_min;
    lamp1_fade.level_max = g_light_config.level_max;
    lamp1_fade.transition_time = g_light_config.transition_time;
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

    double dt = 0.25;

    lamp1_fade.step_table = lamp1_steps;
    lamp2_fade.step_table = lamp2_steps;

    generateBrightnessSteps(&lamp1_fade, dt);
    generateBrightnessSteps(&lamp2_fade, dt);

    // // Print the generated brightness steps for lamp1
    // printf("Lamp 1 Brightness Steps (%d):\n", lamp1_fade.step_table_size);
    // for (int i = 0; i < lamp1_fade.step_table_size; ++i)
    // {
    //     if (lamp1_steps[i].time > 0.0)
    //     {
    //         printf("Time: %.2f, Brightness: %d\n", lamp1_steps[i].time, lamp1_steps[i].brightness);
    //     }
    // }

    // // Print the generated brightness steps for lamp2
    // printf("Lamp 2 Brightness Steps (%d):\n", lamp2_fade.step_table_size);
    // for (int i = 0; i < lamp2_fade.step_table_size; ++i)
    // {
    //     if (lamp2_steps[i].time > 0.0)
    //     {
    //         printf("Time: %.2f, Brightness: %d\n", lamp2_steps[i].time, lamp2_steps[i].brightness);
    //     }
    // }

    // Start tasks
    xTaskCreate(light_fade_move_rtos_task,
                "lamp1_fade_task",
                4096,
                &lamp1_fade,
                4,
                &lamp1_fade.task_handle);

    xTaskCreate(light_fade_move_rtos_task,
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

    // Optionally set them to a default level
    // ...
}
