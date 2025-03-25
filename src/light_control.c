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

/* Some simplified ZCL commands. You can unify them if you like. */
void level_move_with_onoff(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address)
{
    esp_zb_zcl_level_move_cmd_t cmd_move = {0};
    cmd_move.zcl_basic_cmd.src_endpoint = 1;
    cmd_move.zcl_basic_cmd.dst_endpoint = 1;
    cmd_move.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    memcpy(cmd_move.zcl_basic_cmd.dst_addr_u.addr_long, long_address, sizeof(esp_zb_ieee_addr_t));
    cmd_move.move_mode = mode;
    cmd_move.rate = rate;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_with_onoff_cmd_req(&cmd_move);
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

    ESP_LOGD(TAG, "To level %d with transition time %dms for address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             level, transition_time,
             long_address[0], long_address[1], long_address[2], long_address[3],
             long_address[4], long_address[5], long_address[6], long_address[7]);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_cmd_req(&cmd_move_to);
    esp_zb_lock_release();
}

// Function to calculate binomial coefficient
static double binomial_coefficient(unsigned int n, unsigned int k)
{
    double result = 1.0;
    if (k > n - k)
        k = n - k; // Take advantage of symmetry
    for (unsigned int i = 0; i < k; i++)
    {
        result *= (n - i);
        result /= (i + 1);
    }
    return result;
}

static bezier_point_t cubic_bezier_2d(double t, bezier_point_t *ctrl, size_t n)
{
    bezier_point_t out = {0, 0};
    if (n < 2)
        return out; // or mark invalid

    unsigned int degree = n - 1;
    for (unsigned int i = 0; i <= degree; i++)
    {
        double binom = binomial_coefficient(degree, i);
        double bernstein = binom * pow(1 - t, degree - i) * pow(t, i);
        out.x += ctrl[i].x * bernstein;
        out.y += ctrl[i].y * bernstein;
    }
    return out;
}

// We want to solve X(t) = desired_x in [0..1].
// We'll do a simple binary search or Newton's method for t in [0..1].
static double invert_bezier_x(double desired_x,
                              const bezier_point_t *ctrl,
                              size_t n,
                              double epsilon)
{
    double low = 0.0;
    double high = 1.0;
    while (high - low > epsilon)
    {
        double mid = 0.5 * (low + high);
        double xm = cubic_bezier_2d(mid, ctrl, n).x;
        if (xm < desired_x)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    return 0.5 * (low + high);
}
double brightness_at_fraction(double fraction)
{
    double param = invert_bezier_x(fraction, g_bezier_points, g_num_bezier_points, 1e-5);
    bezier_point_t p = cubic_bezier_2d(param, g_bezier_points, g_num_bezier_points);
    return p.y; // That is our brightness
}

double brightness(double t)
{
    // Normalize t to the cycle duration
    double cycle_s = g_light_config.transition_time * 2 + (g_light_config.on_time + g_light_config.off_time) * g_light_config.transition_time;
    double normalized_t = fmod(t, cycle_s) / cycle_s;

    // Adjust the normalized_t to create a back-and-forth effect
    if (normalized_t > 0.5)
    {
        normalized_t = 1.0 - normalized_t; // Reverse direction in the second half of the cycle
    }
    normalized_t *= 2.0; // Scale to maintain full range (0 to 1)

    // Calculate brightness level based on the cubic Bezier curve
    double normalized_brightness = brightness_at_fraction(normalized_t);

    if (normalized_brightness < 0)
        normalized_brightness = 0;
    if (normalized_brightness > 1)
        normalized_brightness = 1;

    // Denormalize the brightness to a scale of 0 to 255
    //double denormalized_brightness = normalized_brightness * 254.0;

    // Return the denormalized brightness level as a double
    return normalized_brightness;
}


uint8_t get_inverse_lut_interpolated(float desired01)
{
    // clamp
    if (desired01 < 0.0f) desired01 = 0.0f;
    if (desired01 > 1.0f) desired01 = 1.0f;

    float indexFloat = desired01 * 255.0f;     // e.g. 0..255
    int indexLow = (int)floorf(indexFloat);    
    int indexHigh = indexLow + 1;
    if (indexHigh > 255) indexHigh = 255;

    float frac = indexFloat - (float)indexLow; // in [0..1)

    // LUT values (these are the recommended commands for each discrete desired brightness)
    uint8_t cmdLow  = g_inverseLUT[indexLow];
    uint8_t cmdHigh = g_inverseLUT[indexHigh];

    // linear interpolation in float
    float cmdF = (1.0f - frac) * cmdLow + frac * cmdHigh;

    // round
    uint8_t cmd = (uint8_t)(cmdF + 0.5f);
    return cmd;
}


static void light_fade_rtos_task(void *pvParameters)
{

    light_fade_t *light_fade = (light_fade_t *)pvParameters;

    uint16_t step_ms = 200; // Convert steptime from the config to milliseconds

    double transition_s = g_light_config.transition_time;
    double on_s = transition_s * g_light_config.on_time;
    double off_s = transition_s * g_light_config.off_time;
    double cycle_s = (2 * transition_s) + on_s + off_s;
    double offset_s = cycle_s * light_fade->offset;
    double t = 0;
    uint8_t prev_level = 0; // Track the previous level

    while (1)
    {
        if (!g_fade_paused)
        {

            double current_brightness = brightness(t + offset_s);
            uint8_t interpolated_brightness = get_inverse_lut_interpolated(current_brightness);

            //uint8_t next_level = (uint8_t)round(current_brightness);

            ESP_LOGI(TAG, "Setting Lamp%d to %d within %dms",
                     light_fade->id, interpolated_brightness, step_ms);

            if (g_light_config.dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL_WITH_OFF_OFF)
            {
                move_to_level_with_onoff(interpolated_brightness, (uint16_t)step_ms / 100, light_fade->address);
            }
            else if (g_light_config.dimming_strategy == DIMMING_STRATEGY_MOVE_TO_LEVEL)
            {
                move_to_level(interpolated_brightness, (uint16_t)step_ms / 100, light_fade->address);
            }
            // else if (g_light_config.dimming_strategy == DIMMING_STRATEGY_LEVEL_MOVE)
            // {
            //     uint8_t mode = (next_level > prev_level) ? 1 : 0;
            //     uint8_t rate = (uint8_t)abs(next_level - prev_level) / (step_ms / 100); // Simplified rate calculation

            //     level_move(mode, rate, light_fade->address);
            // }
            // else if (g_light_config.dimming_strategy == DIMMING_STRATEGY_LEVEL_MOVE_WITH_ON_OFF)
            // {
            //     uint8_t mode = (next_level > prev_level) ? 1 : 0;
            //     uint8_t rate = (uint8_t)abs(next_level - prev_level) / (step_ms / 100); // Simplified rate calculation

            //     level_move_with_onoff(mode, rate, light_fade->address);
            // }

            // Step time delay to apply brightness changes in intervals
            vTaskDelay(step_ms / portTICK_PERIOD_MS);

            if (t > cycle_s)
                t = t - cycle_s;
            ;
            // Increment time based on step_ms
            t += step_ms / 1000.0; // convert ms to seconds

            // Update prev_level to track the last brightness set
            prev_level = interpolated_brightness;
        }
        else
        {
            // If paused, just delay a bit
            vTaskDelay(100 / portTICK_PERIOD_MS);
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
    level_stop(lamp1_fade.address);
    level_stop(lamp2_fade.address);

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

    // Optionally set them to a default level
    // ...
}
