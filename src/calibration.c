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
void fitGammaPowerLawNormalized(
    const float *x, // xScaled array in [0..1]
    const float *y, // yScaled array in [0..1]
    int n,
    float *A,
    float *gamma)
{
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    int validCount = 0;

    for (int i = 0; i < n; i++)
    {
        // Must skip x=0 or y=0 if taking logs:
        // (Or clamp them to a small positive value like 1e-6)
        if (x[i] > 1e-6f && y[i] > 1e-6f)
        {
            double lx = log(x[i]);
            double ly = log(y[i]);
            sumX += lx;
            sumY += ly;
            sumXX += (lx * lx);
            sumXY += (lx * ly);
            validCount++;
        }
    }

    if (validCount < 2)
    {
        // Not enough valid points
        *A = 0.0f;
        *gamma = 1.0f; // fallback
        return;
    }

    double denom = (validCount * sumXX - sumX * sumX);
    if (fabs(denom) < 1e-12)
    {
        *A = 0.0f;
        *gamma = 1.0f;
        return;
    }

    double b = (validCount * sumXY - sumX * sumY) / denom; // b = gamma
    double a = (sumY - b * sumX) / validCount;             // a = ln(A)

    *A = (float)exp(a);
    *gamma = (float)b;
}

// We'll do a direct solution of the 3x3 system for a quadratic fit:
// y = a0 + a1*x + a2*x^2

typedef struct
{
    float a0;
    float a1;
    float a2;
} Poly2Coeffs;

void fitQuadratic(const float *x, const float *y, int n, Poly2Coeffs *out)
{
    double Sx = 0.0;    // sum(x)
    double Sy = 0.0;    // sum(y)
    double Sxx = 0.0;   // sum(x^2)
    double Sxy = 0.0;   // sum(x*y)
    double Sxxx = 0.0;  // sum(x^3)
    double Sxxy = 0.0;  // sum(x^2*y)
    double Sxxxx = 0.0; // sum(x^4)

    int validCount = 0;

    for (int i = 0; i < n; i++)
    {
        double xi = x[i];
        double yi = y[i];
        Sx += xi;
        Sy += yi;
        double xx = xi * xi;
        Sxx += xx;
        Sxy += (xi * yi);
        double xxx = xx * xi;
        Sxxx += xxx;
        Sxxy += (xx * yi);
        Sxxxx += (xx * xx);
        validCount++;
    }
    // We'll call that n2 to avoid confusion
    double n2 = (double)validCount;

    // The normal equations matrix is:
    //
    // [ n2    Sx     Sxx  ] [ a0 ]   [ Sy   ]
    // [ Sx    Sxx    Sxxx ] [ a1 ] = [ Sxy  ]
    // [ Sxx   Sxxx   Sxxxx] [ a2 ]   [ Sxxy ]
    //
    // We'll solve for a0, a1, a2.

    // Let's set up that matrix:
    double M[3][3] = {
        {n2, Sx, Sxx},
        {Sx, Sxx, Sxxx},
        {Sxx, Sxxx, Sxxxx}};
    double RHS[3] = {Sy, Sxy, Sxxy};

    // Now solve the 3x3 system M * A = RHS (where A = [a0, a1, a2]).
    // You can write or use a small function to do a 3x3 solve via e.g. Gaussian elimination.

    // A quick-and-dirty approach (not super robust for degenerate data):
    // We'll do a simple partial pivot approach. For production code,
    // ensure you handle edge cases carefully.

    // Convert to float in the end
    double a0 = 0, a1 = 0, a2 = 0;

    // We'll implement a small inline for 3x3 pivot/solve:
    {
        // Forward elimination
        for (int k = 0; k < 3; k++)
        {
            // Pivot selection (find row with max M[row][k])
            double maxVal = fabs(M[k][k]);
            int pivot = k;
            for (int r = k + 1; r < 3; r++)
            {
                double val = fabs(M[r][k]);
                if (val > maxVal)
                {
                    maxVal = val;
                    pivot = r;
                }
            }
            // Swap pivot row if needed
            if (pivot != k)
            {
                for (int c = 0; c < 3; c++)
                {
                    double tmp = M[k][c];
                    M[k][c] = M[pivot][c];
                    M[pivot][c] = tmp;
                }
                double tmpR = RHS[k];
                RHS[k] = RHS[pivot];
                RHS[pivot] = tmpR;
            }
            // Eliminate below pivot
            for (int r = k + 1; r < 3; r++)
            {
                double factor = M[r][k] / M[k][k];
                for (int c = k; c < 3; c++)
                {
                    M[r][c] -= factor * M[k][c];
                }
                RHS[r] -= factor * RHS[k];
            }
        }
        // Back substitution
        for (int i = 2; i >= 0; i--)
        {
            double sumv = RHS[i];
            for (int c = i + 1; c < 3; c++)
            {
                sumv -= M[i][c] * RHS[c];
            }
            RHS[i] = sumv / M[i][i];
        }
        // Now RHS[0..2] holds a0, a1, a2
        a0 = RHS[0];
        a1 = RHS[1];
        a2 = RHS[2];
    }

    out->a0 = (float)a0;
    out->a1 = (float)a1;
    out->a2 = (float)a2;
}

int main(void)
{
    // Example data:
    static float xData[256], yData[256];
    for (int i = 0; i < 256; i++)
    {
        xData[i] = (float)i;
        // Let the real function be y=5 + 2*x + 0.01*x^2, plus some noise
        float trueVal = 5.0f + 2.0f * i + 0.01f * (i * i);
        // Add small noise if desired
        yData[i] = trueVal;
    }

    Poly2Coeffs coeffs;
    fitQuadratic(xData, yData, 256, &coeffs);

    printf("Fitted polynomial: y = %.4f + %.4f*x + %.4f*x^2\n",
           coeffs.a0, coeffs.a1, coeffs.a2);

    // Example usage:
    float testX = 100.0f;
    float predictedY = coeffs.a0 + coeffs.a1 * testX + coeffs.a2 * (testX * testX);
    printf("Predicted Y for x=100 is %f\n", predictedY);

    return 0;
}

void start_calibration()
{

    // start_light_sensor_task();
    long int min_value, max_value;
    ESP_LOGI(TAG, "Stopping Fade...");
    lights_stop();
    move_to_level_with_onoff(0, 0, lamp2_long_address);
    ESP_LOGI(TAG, "Moving to full brightness");
    move_to_level_with_onoff(10, 0, lamp1_long_address);
    vTaskDelay(pdMS_TO_TICKS(100));
    move_to_level(0, 0, lamp1_long_address);

    // Move to max brightness (e.g. 254), measure
    vTaskDelay(pdMS_TO_TICKS(1000));
    static float yData[256];

    ESP_LOGI(TAG, "Looping through brightness levels...");

    for (int i = 0; i <= 256; ++i)
    {
        ESP_LOGI(TAG, "Setting brightness to: %d", i);
        move_to_level(i, 0, lamp1_long_address);
        vTaskDelay(pdMS_TO_TICKS(200));
        yData[i] = get_averaged_sensor_reading();
        ESP_LOGI(TAG, "brightness: %d, reading: %f", i, yData[i]);
    }

    float sensorMin = 999999.0f;
    float sensorMax = -999999.0f;

    for (int i = 0; i < 256; i++)
    {
        if (yData[i] < sensorMin)
            sensorMin = yData[i];
        if (yData[i] > sensorMax)
            sensorMax = yData[i];
    }
    float sensorRange = sensorMax - sensorMin;

    static float xScaled[256], yScaled[256];
    for (int i = 0; i < 256; i++)
    {
        xScaled[i] = (float)i / 255.0f; // Now in [0..1]

        // Shift and scale the sensor data so that the minimum reading maps to 0.0
        // and the maximum reading maps to 1.0
        yScaled[i] = (yData[i] - sensorMin) / sensorRange; // Also in [0..1]
    }
    float A_fit, gamma_fit;
    fitGammaPowerLawNormalized(xScaled, yScaled, 256, &A_fit, &gamma_fit);
    printf("Fitted in normalized space: yScaled = %.3f * xScaled^(%.3f)\n",
           A_fit, gamma_fit);

    Poly2Coeffs coeffs;
    fitQuadratic(xScaled, yScaled, 256, &coeffs);
    printf("Fitted polynomial: y = %.4f + %.4f*x + %.4f*x^2\n",
           coeffs.a0, coeffs.a1, coeffs.a2);
    // Example usage:
    float testX = 100.0f;
    float predictedY = coeffs.a0 + coeffs.a1 * testX + coeffs.a2 * (testX * testX);
    printf("Predicted Y for x=100 is %f\n", predictedY);

    lights_init();
}
