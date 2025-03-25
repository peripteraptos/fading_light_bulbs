#include "light_control.h"
#include "app_config.h"
#include "light_sensor.h"

static char *TAG = "CALIBRATION";

long int get_averaged_sensor_reading()
{
    long long int sum = 0;
    for (int i = 0; i < 100; ++i)
    {
        sum += get_current_value();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return (long int)(sum / 100);
}

long int get_max_sensor_reading()
{
    long int max = 0;
    long int current;
    for (int i = 0; i < 100; ++i)
    {
        current = get_current_value();
        if (max < current)
        {
            max = current;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return max;
}



float get_stable_sensor_reading(int maxTries, int sampleCount, float maxStdDevAllowed)
{
    // We'll take sampleCount readings, compute mean & stdev.
    // If stdev is too large, we can optionally retry (up to maxTries times).

    float mean = 0.0f;
    float stdDev = 0.0f;

    for (int attempt = 0; attempt < maxTries; attempt++)
    {
        // Gather samples
        float sum = 0.0f;
        float sumSq = 0.0f;
        int validSamples = 0;
        const int delayMs = 10; // time between samples (adjust as needed)

        // Collect data
        for (int i = 0; i < sampleCount; i++)
        {
            float val = (float)get_current_value();
            sum += val;
            sumSq += (val * val);
            validSamples++;

            // small delay to let the ADC settle
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }

        // Compute mean
        mean = sum / (float)validSamples;

        // Compute variance = (sum(x^2) / n) - mean^2
        float variance = (sumSq / (float)validSamples) - (mean * mean);
        stdDev = sqrtf(variance);

        if (stdDev <= maxStdDevAllowed)
        {
            // Good enough, break out
            break;
        }
        else
        {
            // Possibly log a warning or debug message
            printf("Attempt %d: stdev=%.2f above threshold=%.2f, retrying...\n",
                   attempt, stdDev, maxStdDevAllowed);
            // next iteration will try again
        }
    }

    // Optionally, if still above threshold after all tries, you could do fallback:
    // e.g. accept the last average or return some error code.
    // We'll just return the last average here.
    return mean;
}


static float yMeasured[256];     // raw measured brightness in [some range]
static float yScaled[256];       // normalized to [0..1]
               

static float buildInverseTable(void)
{
    // 1) find min & max among yMeasured
    float minVal = 1e30f, maxVal = -1e30f;
    for (int i = 0; i < 256; i++)
    {
        if (yMeasured[i] < minVal) minVal = yMeasured[i];
        if (yMeasured[i] > maxVal) maxVal = yMeasured[i];
    }
    float range = maxVal - minVal;
    if (range < 1e-9f) range = 1.0f; // fallback if all data are identical

    // 2) build normalized yScaled
    for (int i = 0; i < 256; i++)
    {
        yScaled[i] = (yMeasured[i] - minVal) / range; // in [0..1]
    }

    // 3) build inverse LUT
    // g_inverseLUT[dIndex] = best input i for desired brightness = dIndex/255
    for (int dIndex = 0; dIndex < 256; dIndex++)
    {
        float desired = (float)dIndex / 255.0f;
        float bestDiff = 999999.0f;
        int bestI = 0;
        for (int i = 0; i < 256; i++)
        {
            float diff = fabsf(yScaled[i] - desired);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestI = i;
            }
        }
        g_inverseLUT[dIndex] = (uint8_t)bestI;
    }
    return minVal; // e.g. might want to return something else, or nothing
}

void start_calibration()
{
    ESP_LOGI(TAG, "Starting calibration...");
     
    lights_stop();
    //Turn lamp2 off first
    move_to_level_with_onoff(0, 0, lamp2_long_address);
    move_to_level_with_onoff(10, 0, lamp1_long_address);

    move_to_level(0, 0, lamp1_long_address);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 1) measure brightness from 0..255
    for (int i = 0; i < 256; i++)
    {
        move_to_level((uint8_t)i, 0, lamp1_long_address);
        vTaskDelay(pdMS_TO_TICKS(150)); // let brightness settle

        // We'll gather a stable sensor reading:
        float reading = get_stable_sensor_reading(/*maxTries=*/3,
                                                  /*sampleCount=*/50,
                                                  /*maxStdDevAllowed=*/5.0f);
        yMeasured[i] = reading;
        ESP_LOGI(TAG, "Level=%3d -> ADC=%.2f", i, reading);
        }
    // 2) Build inverse table
    buildInverseTable();

    // Print the inverse LUT table
    ESP_LOGI(TAG, "Inverse LUT Table:");
    for (int i = 0; i < 256; i++) {
        ESP_LOGI(TAG, "%3d -> %3d", i, g_inverseLUT[i]);
    }
    // Done
    ESP_LOGI(TAG, "Calibration complete.");
    lights_init();
}