#pragma once

/**
 * @brief Initializes all console commands (get, set, start_calibration).
 */
void register_console_commands(void);

 int cmd_get_config(int argc, char **argv);
