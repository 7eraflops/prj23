#include "board_manager.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "BoardManager";

namespace board_manager {

struct InputContext {
    gpio_num_t gpio_num;
    uint32_t hold_time_ms;
    std::function<void()> callback;
};

static void input_task(void* pvParameters) {
    InputContext* ctx = static_cast<InputContext*>(pvParameters);
    
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ctx->gpio_num);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    uint32_t press_start_time = 0;
    bool is_pressed = false;

    while (true) {
        int level = gpio_get_level(ctx->gpio_num);
        bool currently_pressed = (level == 0);

        if (currently_pressed && !is_pressed) {
            is_pressed = true;
            press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        } else if (!currently_pressed && is_pressed) {
            is_pressed = false;
        }

        if (is_pressed) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - press_start_time >= ctx->hold_time_ms) {
                ESP_LOGI(TAG, "Reset button long press detected!");
                if (ctx->callback) {
                    ctx->callback();
                }
                while (gpio_get_level(ctx->gpio_num) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                is_pressed = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t init_reset_button(gpio_num_t gpio_num, uint32_t hold_time_ms, std::function<void()> on_long_press) {
    InputContext* ctx = new InputContext{gpio_num, hold_time_ms, on_long_press};
    BaseType_t ret = xTaskCreate(input_task, "board_input_task", 4096, ctx, 10, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

} // namespace board_manager
