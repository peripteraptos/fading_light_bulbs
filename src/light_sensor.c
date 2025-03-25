/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include "light_sensor.h"
 #include <string.h>
 #include <stdio.h>
 #include "esp_log.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/semphr.h"
 #include "esp_adc/adc_continuous.h"
 
 #define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
 #define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
 #define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
 #define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
 #define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_12
 #define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

 #define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
 #define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
 #define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
 #define EXAMPLE_READ_LEN                    512

#define EXAMPLE_ADC1_CHAN0 ADC_CHANNEL_6
#define EXAMPLE_ADC_ATTEN  ADC_ATTEN_DB_6
 


 static adc_channel_t channel[1] = {ADC_CHANNEL_6};

 
 static TaskHandle_t s_task_handle;
 static const char *TAG = "LIGHT_SENSOR";

 long int current_value = 0;


 long int get_current_value(void){
    return current_value;
 }
 
 static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
 {
     BaseType_t mustYield = pdFALSE;
     //Notify that ADC continuous driver has done enough number of conversions
     vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
 
     return (mustYield == pdTRUE);
 }
 
 static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
 {
     adc_continuous_handle_t handle = NULL;
 
     adc_continuous_handle_cfg_t adc_config = {
         .max_store_buf_size = 2048,
         .conv_frame_size = EXAMPLE_READ_LEN,
     };
     ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));
 
     adc_continuous_config_t dig_cfg = {
         .sample_freq_hz = 20 * 1000,
         .conv_mode = EXAMPLE_ADC_CONV_MODE,
         .format = EXAMPLE_ADC_OUTPUT_TYPE,
     };
 
     adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
     dig_cfg.pattern_num = channel_num;
     for (int i = 0; i < channel_num; i++) {
         adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
         adc_pattern[i].channel = channel[i] & 0x7;
         adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
         adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;
 
         ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
         ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
         ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
     }
     dig_cfg.adc_pattern = adc_pattern;
     ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
 
     *out_handle = handle;
 }
 
 static void light_sensor_rtos_task(void *pvParameters)
 {
     esp_err_t ret;
     uint32_t ret_num = 0;
     uint8_t result[EXAMPLE_READ_LEN] = {0};
     memset(result, 0xcc, EXAMPLE_READ_LEN);
 
     s_task_handle = xTaskGetCurrentTaskHandle();
 
     adc_continuous_handle_t handle = NULL;
     continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);
 
     adc_continuous_evt_cbs_t cbs = {
         .on_conv_done = s_conv_done_cb,
     };
     ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
     ESP_ERROR_CHECK(adc_continuous_start(handle));
 
     while (1) {
 
         /**
          * This is to show you the way to use the ADC continuous mode driver event callback.
          * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
          * However in this example, the data processing (print) is slow, so you barely block here.
          *
          * Without using this event callback (to notify this task), you can still just call
          * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
          */
         ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
 
         char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);
 
         uint32_t sum = 0;
         uint32_t count = 0;

         while (1) {
             ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
             if (ret == ESP_OK) {
                 for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                     adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                     uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                     uint32_t data = EXAMPLE_ADC_GET_DATA(p);

                     /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
                     if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT)) {
                         sum += data;
                         count++;

                         /* Log average value every 100 readings */
                         if (count == 1024) {
                             uint32_t average = sum / 1024;
                             current_value = average;
                             ESP_LOGI(TAG, "Value: %ld", average);
                             sum = 0;
                             count = 0;
                             vTaskDelay(1);
                         }
                     } else {
                         ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
                     }
                 }

                 /* Add a short delay to prevent task watchdog timeout as needed */
                 vTaskDelay(1);
             } else if (ret == ESP_ERR_TIMEOUT) {
                 // We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                 break;
             }
         }
     }
     
     ESP_ERROR_CHECK(adc_continuous_stop(handle));
     ESP_ERROR_CHECK(adc_continuous_deinit(handle));
 }

 void start_light_sensor_task(void)
{
    if (s_task_handle != NULL){
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
    xTaskCreate(light_sensor_rtos_task, "light_sensor_rtos_task", 8168, NULL, 3, NULL);
}

 void stop_light_sensor_task(void)
{
    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
}