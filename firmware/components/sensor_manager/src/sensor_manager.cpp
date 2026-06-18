#include "sensor_manager.hpp"

#include <cstdio>
#include <esp_log.h>
#include <string>

static const char* TAG = "SENSOR_MANAGER";

SensorManager::SensorManager(std::unique_ptr<IEnergySensor> sensor, MqttManager& mqtt_manager,
                             int num_channels)
    : _sensor(std::move(sensor)), _mqtt_manager(mqtt_manager), _num_channels(num_channels) {}

SensorManager::~SensorManager() {
    stop();
}

bool SensorManager::start() {
    if (_running) {
        ESP_LOGW(TAG, "SensorManager is already running.");
        return false;
    }

    if (!_sensor) {
        ESP_LOGE(TAG, "No sensor implementation provided.");
        return false;
    }

    if (!_sensor->init()) {
        ESP_LOGE(TAG, "Failed to initialize sensor.");
        return false;
    }

    _running.store(true);
    BaseType_t ret = xTaskCreate(task_entry, "sensor_telemetry_task", 4096, this,
                                 5, // Priority
                                 &_task_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task.");
        _running.store(false);
        return false;
    }

    _stop_sem = xSemaphoreCreateBinary();

    ESP_LOGI(TAG, "SensorManager started successfully.");
    return true;
}

void SensorManager::stop() {
    if (!_running.exchange(false)) {
        return;
    }

    ESP_LOGI(TAG, "Stopping SensorManager...");

    if (_stop_sem) {
        if (xSemaphoreTake(_stop_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGW(TAG, "Sensor task did not stop in time, force deleting");
            if (_task_handle) {
                vTaskDelete(_task_handle);
            }
        }
        vSemaphoreDelete(_stop_sem);
        _stop_sem = nullptr;
    }
    
    if (_sensor) {
        _sensor->save_state();
    }
    
    _task_handle = nullptr;

    ESP_LOGI(TAG, "SensorManager stopped.");
}

void SensorManager::task_entry(void* arg) {
    auto* instance = static_cast<SensorManager*>(arg);
    instance->task_loop();
    if (instance->_stop_sem) {
        xSemaphoreGive(instance->_stop_sem);
    }
    vTaskDelete(NULL);
}

void SensorManager::task_loop() {
    ESP_LOGI(TAG, "Telemetry task running.");
    TickType_t last_wake = xTaskGetTickCount();

    while (_running.load()) {
        _sensor->update();

        if (_mqtt_manager.is_connected()) {
            publish_telemetry();
        } else {
            ESP_LOGD(TAG, "MQTT not connected. Skipping telemetry publish.");
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Telemetry task exiting.");
}

void SensorManager::publish_telemetry() {
    char payload[256];

    for (int i = 0; i < _num_channels; ++i) {
        EnergyData data = _sensor->read_channel(i);

        // Format payload as JSON
        int len = snprintf(
            payload, sizeof(payload),
            "{\"voltage\":%.2f,\"current\":%.3f,\"active_power\":%.2f,\"apparent_power\":%.2f,"
            "\"reactive_power\":%.2f,\"power_factor\":%.2f,\"energy\":%.6f,\"frequency\":%.2f}",
            data.voltage, data.current, data.active_power, data.apparent_power, data.reactive_power,
            data.power_factor, data.energy, data.frequency);

        if (len > 0 && len < sizeof(payload)) {
            std::string topic = "energy_meter/channel_" + std::to_string(i) + "/state";

            // Publish with QoS 0, no retain (high frequency data)
            _mqtt_manager.publish(topic, payload, 0, false);
            
            // Slight delay to prevent flooding the MQTT broker queue (batching)
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "Failed to format JSON payload for channel %d", i);
        }
    }
}