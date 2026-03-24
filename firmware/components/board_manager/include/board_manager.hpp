#pragma once

#include <esp_err.h>
#include <driver/gpio.h>
#include <functional>

namespace board_manager {

/**
 * @brief Initialize the board manager and start monitoring the specified GPIO for a long press.
 */
esp_err_t init_reset_button(gpio_num_t gpio_num, uint32_t hold_time_ms, std::function<void()> on_long_press);

} // namespace board_manager
