#include "sensor_manager.hpp"
#include <esp_log.h>
#include <cstdio>
#include <string>

static const char* TAG = "SENSOR_MANAGER";

SensorManager::SensorManager(std::unique_ptr<IEnergySensor> sensor, MqttManager& mqtt_manager, int num_channels)
    : _sensor(std::move(sensor)), _mqtt_manager(mqtt_manager), _num_channels(num_channels),
      _task_handle(nullptr), _running(false) {
}

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

    _running = true;
    BaseType_t ret = xTaskCreate(
        task_entry,
        "sensor_telemetry_task",
        4096,
        this,
        5, // Priority
        &_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task.");
        _running = false;
        return false;
    }

    ESP_LOGI(TAG, "SensorManager started successfully.");
    return true;
}

void SensorManager::stop() {
    if (!_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping SensorManager...");
    _running = false;
    
    // Give the task a moment to exit naturally
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    if (_task_handle != nullptr) {
        vTaskDelete(_task_handle);
        _task_handle = nullptr;
    }
    
    ESP_LOGI(TAG, "SensorManager stopped.");
}

void SensorManager::task_entry(void* arg) {
    SensorManager* instance = static_cast<SensorManager*>(arg);
    instance->task_loop();
    
    // If the loop exits, clean up the task handle
    instance->_task_handle = nullptr;
    vTaskDelete(NULL);
}

void SensorManager::task_loop() {
    ESP_LOGI(TAG, "Telemetry task running.");
    
    while (_running) {
        // Update sensor data (read from physical or dummy sensor)
        _sensor->update();
        
        // Publish telemetry if MQTT is connected
        if (_mqtt_manager.is_connected()) {
            publish_telemetry();
        } else {
            ESP_LOGD(TAG, "MQTT not connected. Skipping telemetry publish.");
        }

        // Wait 1 second before next update
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Telemetry task exiting.");
}

void SensorManager::publish_telemetry() {
    char payload[256];
    
    for (int i = 0; i < _num_channels; ++i) {
        EnergyData data = _sensor->read_channel(i);
        
        // Format payload as JSON
        int len = snprintf(payload, sizeof(payload), 
            "{\"voltage\":%.2f,\"current\":%.3f,\"active_power\":%.2f,\"apparent_power\":%.2f,\"reactive_power\":%.2f,\"power_factor\":%.2f,\"energy\":%.6f,\"frequency\":%.2f}",
            data.voltage,
            data.current,
            data.active_power,
            data.apparent_power,
            data.reactive_power,
            data.power_factor,
            data.energy,
            data.frequency
        );
        
        if (len > 0 && len < sizeof(payload)) {
            std::string topic = "energy_meter/channel_" + std::to_string(i) + "/state";
            
            // Publish with QoS 0, no retain (high frequency data)
            _mqtt_manager.publish(topic, payload, 0, false);
        } else {
            ESP_LOGE(TAG, "Failed to format JSON payload for channel %d", i);
        }
    }
}