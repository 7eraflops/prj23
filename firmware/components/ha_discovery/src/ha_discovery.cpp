#include "ha_discovery.hpp"

#include <cstdio>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sstream>

static const char* TAG = "HA_DISCOVERY";

HaDiscovery::HaDiscovery(MqttManager& mqtt_manager, const std::string& device_id)
    : _mqtt_manager(mqtt_manager), _status_topic("energy_meter/heartbeat"), _device_id(device_id) {}

void HaDiscovery::publish_discovery_messages(int num_channels) {
    if (!_mqtt_manager.is_connected()) {
        ESP_LOGW(TAG, "Cannot publish discovery messages: MQTT not connected.");
        return;
    }

    ESP_LOGI(TAG, "Publishing Home Assistant discovery messages for %d channels...", num_channels);

    for (int i = 0; i < num_channels; ++i) {
        publish_sensor_discovery(i, "voltage", "Voltage", "V", "voltage", "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "current", "Current", "A", "current", "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "active_power", "Power", "W", "power", "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "apparent_power", "Apparent Power", "VA", "apparent_power",
                                 "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "reactive_power", "Reactive Power", "var", "reactive_power",
                                 "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "power_factor", "Power Factor", "", "power_factor",
                                 "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "energy", "Energy", "kWh", "energy", "total_increasing");
        vTaskDelay(pdMS_TO_TICKS(10));
        publish_sensor_discovery(i, "frequency", "Frequency", "Hz", "frequency", "measurement");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    publish_diagnostic_discovery("wifi_rssi", "Wi-Fi Signal", "dBm", "signal_strength",
                                 "diagnostic");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_diagnostic_discovery("free_heap", "Free Memory", "B", "data_size", "diagnostic");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_diagnostic_discovery("uptime", "Uptime", "h", "duration", "diagnostic");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_diagnostic_discovery("mcu_temp", "MCU Temperature", "°C", "temperature", "diagnostic");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_diagnostic_discovery("stack_min", "Stack High Watermark", "B", "data_size", "diagnostic");

    ESP_LOGI(TAG, "Finished publishing discovery messages.");
}

void HaDiscovery::publish_sensor_discovery(int channel, const std::string& sensor_type,
                                           const std::string& name, const std::string& unit,
                                           const std::string& device_class,
                                           const std::string& state_class) {
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_ch%d_%s/config", _device_id.c_str(),
             channel, sensor_type.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"Channel " << (channel + 1) << " " << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_ch" << channel << "_" << sensor_type << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_ch" << channel << "_" << sensor_type << "\",";
    payload << "\"state_topic\": \"energy_meter/channel_" << channel << "/state\",";
    if (!unit.empty()) {
        payload << "\"unit_of_measurement\": \"" << unit << "\",";
    }
    if (!device_class.empty()) {
        payload << "\"device_class\": \"" << device_class << "\",";
    }
    payload << "\"state_class\": \"" << state_class << "\",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";
    payload << "\"value_template\": \"{{ value_json." << sensor_type << " }}\",";

    // Device info (Sub-device per channel)
    payload << "\"device\": {";
    payload << "\"identifiers\": [\"" << _device_id << "_ch" << channel << "\"],";
    payload << "\"name\": \"Channel " << (channel + 1) << "\",";
    payload << "\"via_device\": \"" << _device_id << "\",";
    payload << "\"manufacturer\": \"Custom\",";
    payload << "\"model\": \"12-Channel Meter\"";
    payload << "}";
    payload << "}";

    // Publish config message with retain flag = true
    _mqtt_manager.publish(topic, payload.str(), 0, true);
}

void HaDiscovery::publish_diagnostic_discovery(const std::string& sensor_type,
                                               const std::string& name, const std::string& unit,
                                               const std::string& device_class,
                                               const std::string& entity_category,
                                               const std::string& state_class) {
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_%s/config", _device_id.c_str(),
             sensor_type.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"" << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_" << sensor_type << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_" << sensor_type << "\",";
    payload << "\"state_topic\": \"" << _status_topic << "\",";
    if (!unit.empty()) {
        payload << "\"unit_of_measurement\": \"" << unit << "\",";
    }
    if (!device_class.empty()) {
        payload << "\"device_class\": \"" << device_class << "\",";
    }
    if (!state_class.empty()) {
        payload << "\"state_class\": \"" << state_class << "\",";
    }
    payload << "\"entity_category\": \"" << entity_category << "\",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";
    payload << "\"value_template\": \"{{ value_json." << sensor_type << " }}\",";

    // Device info
    payload << "\"device\": {";
    payload << "\"identifiers\": [\"" << _device_id << "\"],";
    payload << "\"name\": \"ESP32 Energy Meter\",";
    payload << "\"manufacturer\": \"Custom\",";
    payload << "\"model\": \"12-Channel Meter\"";
    payload << "}";
    payload << "}";

    // Publish config message with retain flag = true
    _mqtt_manager.publish(topic, payload.str(), 0, true);
}