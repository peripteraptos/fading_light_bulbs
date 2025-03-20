#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "zigbee_main.h"
#include "light_control.h"
#include "app_config.h"

static const char *TAG = "ZIGBEE_MAIN";

/*
 * For demonstration, we define example lamp addresses here.
 * Replace with your actual lamp addresses or discover them dynamically.
 */
// static esp_zb_ieee_addr_t lamp1_long_address = {0x5d, 0x23, 0x38, 0xfe, 0xff, 0xf8, 0xe2, 0x44};
// static esp_zb_ieee_addr_t lamp2_long_address = {0xd0, 0x46, 0x3a, 0xfe, 0xff, 0xf8, 0xe2, 0x44};

/*
 * Switch or button logic to toggle fade, etc.
 * Implement as needed for your hardware.
 */
// static bool fade_paused = false;

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    if (esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Zigbee BDB commissioning (mode: %d)", mode_mask);
    }
}

/**
 * @brief Zigbee application signal handler.
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Device started or rebooted. Factory-new? %s",
                     esp_zb_bdb_is_factory_new() ? "Yes" : "No");
            if (esp_zb_bdb_is_factory_new())
            {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            }
            else
            {
                // Already part of a network
                ESP_LOGI(TAG, "Opening network for steering...");
                esp_zb_bdb_open_network(180);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Retry commissioning, status: %s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Formed network successfully");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_NETWORK_STEERING);
        }
        else
        {
            ESP_LOGI(TAG, "Restart network formation, status: %s", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        /* You could capture the newly announced device address if needed */
        // ...
        break;

    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK)
        {
            uint8_t duration = *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (duration)
            {
                ESP_LOGI(TAG, "Network open for %d seconds for joining", duration);
            }
            else
            {
                ESP_LOGW(TAG, "Network closed for joining");
            }
        }
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: %d, status: %s", sig_type, esp_err_to_name(err_status));
        break;
    }
}

/**
 * @brief The main Zigbee stack task. This is typically run in a dedicated FreeRTOS task.
 */
static void zigbee_task(void *pvParameters)
{
    /* Standard Zigbee coordinator configuration */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Set channels, register endpoints, etc. */
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ESP_ZB_GATEWAY_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
        .app_device_version = 0,
    };

    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_ep_list_add_gateway_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);

    /* Start Zigbee Stack in non-blocking mode.
       The main loop is in esp_zb_stack_main_loop(). */
    ESP_ERROR_CHECK(esp_zb_start(false));

    esp_zb_stack_main_loop();
}

/**
 * @brief Public function to start the Zigbee stack task.
 */
void zigbee_start_stack(void)
{
    xTaskCreate(&zigbee_task, "zigbee_task", 4096, NULL, 5, NULL);
}
