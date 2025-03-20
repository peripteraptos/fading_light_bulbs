
#include "esp_log.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "app_config.h"
#include "zigbee_main.h"
#include "light_control.h"
#include "console_cmd.h"
#include "calibration.h"


static const char *TAG = "APP_MAIN";

#define PROMPT_STR "ZB_Dimmer"

/* Example of configuring two pins for some special purpose (like your original code). */
static void configure_gpio_pins(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_4, 1);

    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_5);
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_5, 0);
}

void app_main() {

    ESP_LOGI(TAG, "Starting application...");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize console REPL (UART or USB-JTAG, etc.)
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_repl_t *repl = NULL;
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    // Register console commands
    esp_console_register_help_command();
    register_console_commands();

    // Optional: configure special GPIO pins
    configure_gpio_pins();

    // Start the Zigbee stack
    zigbee_start_stack();

    // Start the console REPL so we can type commands (get, set, start_calibration, etc.)
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // Optionally, you can start the default fade tasks right away:
    lights_init();

    // Done! The rest runs in tasks (ZB task, console REPL, etc.).
}