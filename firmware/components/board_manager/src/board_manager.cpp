#include "board_manager.hpp"

#include <driver/gpio.h>
#include <driver/temperature_sensor.h>
#include <esp_timer.h>

#include <esp_log.h>

static const char* TAG = "BoardManager";
static temperature_sensor_handle_t temp_sensor = NULL;

namespace board_manager {

static esp_timer_handle_t s_long_press_timer;
static std::function<void()> s_long_press_callback;
static gpio_num_t s_button_gpio;
static uint64_t s_hold_time_us;

static void long_press_timer_callback(void* arg) {
    if (gpio_get_level(s_button_gpio) == 0) {
        ESP_LOGI(TAG, "Reset button long press detected!");
        if (s_long_press_callback) {
            s_long_press_callback();
        }
    }
}

static void IRAM_ATTR button_isr_handler(void* arg) {
    int level = gpio_get_level(s_button_gpio);
    if (level == 0) {
        esp_timer_start_once(s_long_press_timer, s_hold_time_us);
    } else {
        esp_timer_stop(s_long_press_timer);
    }
}

void init_temperature_sensor() {
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        temperature_sensor_enable(temp_sensor);
        ESP_LOGI(TAG, "MCU Temperature sensor initialized.");
    } else {
        ESP_LOGW(TAG, "Failed to install temperature sensor");
        temp_sensor = NULL;
    }
}

float get_mcu_temperature() {
    float mcu_temp = 0.0f;
    if (temp_sensor) {
        temperature_sensor_get_celsius(temp_sensor, &mcu_temp);
    }
    return mcu_temp;
}

esp_err_t init_reset_button(gpio_num_t gpio_num, uint32_t hold_time_ms,
                            std::function<void()> on_long_press) {
    s_button_gpio = gpio_num;
    s_long_press_callback = std::move(on_long_press);
    s_hold_time_us = (uint64_t)hold_time_ms * 1000;

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = long_press_timer_callback;
    timer_args.arg = NULL;
    timer_args.name = "long_press_timer";
    timer_args.skip_unhandled_events = true;

    esp_err_t err = esp_timer_create(&timer_args, &s_long_press_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create long-press timer: %s", esp_err_to_name(err));
        return err;
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << gpio_num);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(gpio_num, button_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

} // namespace board_manager
