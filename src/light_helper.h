#pragma once
#include "zigbee_main.h"
#include "esp_log.h"
#include <string.h>

void level_move(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address);
void level_move_with_onoff(uint8_t mode, uint8_t rate, esp_zb_ieee_addr_t long_address);
void level_stop(esp_zb_ieee_addr_t long_address);
void move_to_level_with_onoff(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address);
void move_to_level(uint8_t level, uint16_t transition_time, esp_zb_ieee_addr_t long_address);
void move_to_level_immediate(uint8_t level, esp_zb_ieee_addr_t addr);
