#pragma once

#include "esp_zigbee_core.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Zigbee configuration */
#define MAX_CHILDREN 10                     /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE false     /* enable the install code policy for security */
#define HA_COLOR_DIMMABLE_SWITCH_ENDPOINT 1 /* esp light switch device endpoint */
#define HA_GATEWAY_ENDPOINT 2
#define ESP_ZB_PRIMARY_CHANNEL_MASK (1l << 13) /* Zigbee primary channel mask use in the example */
#define ESP_ZB_GATEWAY_ENDPOINT 1              /* Gateway endpoint identifier */
#define APP_PROD_CFG_CURRENT_VERSION 0x0001    /* Production configuration version */

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09" \
                              "ESPRESSIF"             /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07" CONFIG_IDF_TARGET /* Customized model identifier */

#define ESP_ZB_ZC_CONFIG()                                \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,    \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg.zczr_cfg = {                             \
            .max_children = MAX_CHILDREN,                 \
        },                                                \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()       \
    {                                       \
        .radio_mode = ZB_RADIO_MODE_NATIVE, \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                          \
    {                                                         \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, \
    }

/**
 * @brief Starts the Zigbee stack task.
 * This function is intended to be called from app_main().
 */
void zigbee_start_stack(void);

#ifdef __cplusplus
}
#endif
