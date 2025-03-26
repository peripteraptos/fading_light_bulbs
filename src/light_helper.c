#include "light_helper.h"


static const char *TAG = "ZIGBEE";

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



void move_to_level_immediate(uint8_t level, esp_zb_ieee_addr_t addr)
{
    // For a “snap” update, set transition_time=0
    move_to_level_with_onoff(level, 0, addr);
}
