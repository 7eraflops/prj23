#include "ha_discovery.hpp"

#include <cstdio>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sstream>

static const char* TAG = "HA_DISCOVERY";

HaDiscovery::HaDiscovery(MqttManager& mqtt_manager, const std::string& device_id)
    : _mqtt_manager(mqtt_manager), _status_topic("energy_meter/heartbeat"), _device_id(device_id) {}

void HaDiscovery::append_main_device_json(std::ostringstream& ss) const {
    ss << "\"device\": {";
    ss << "\"identifiers\": [\"" << _device_id << "\"],";
    ss << "\"name\": \"ESP32 Energy Meter\",";
    ss << "\"manufacturer\": \"Custom\",";
    ss << "\"model\": \"12-Channel Meter\",";
    ss << "\"configuration_url\": \"http://energy-meter.local\"";
    ss << "}";
}

void HaDiscovery::append_channel_device_json(std::ostringstream& ss, int channel) const {
    ss << "\"device\": {";
    ss << "\"identifiers\": [\"" << _device_id << "_ch" << channel << "\"],";
    ss << "\"name\": \"Channel " << (channel + 1) << "\",";
    ss << "\"via_device\": \"" << _device_id << "\",";
    ss << "\"manufacturer\": \"Custom\",";
    ss << "\"model\": \"12-Channel Meter\"";
    ss << "}";
}

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
    publish_diagnostic_discovery("uptime", "Uptime", "s", "duration", "diagnostic");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_diagnostic_discovery("mcu_temp", "MCU Temperature", "°C", "temperature", "diagnostic");

    // Publish control entities (buttons, switches, numbers, selects)
    publish_control_entities(num_channels);

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
    append_channel_device_json(payload, channel);
    payload << "}";

    // Publish config message with retain flag = true
    _mqtt_manager.publish(topic, payload.str(), 1, true);
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
    append_main_device_json(payload);
    payload << "}";

    // Publish config message with retain flag = true
    _mqtt_manager.publish(topic, payload.str(), 1, true);
}

void HaDiscovery::publish_control_entities(int num_channels) {
    ESP_LOGI(TAG, "Publishing control entities...");

    publish_button_entity("Reboot", "energy_meter/cmnd/reboot", "mdi:restart");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_button_entity("Factory Reset", "energy_meter/cmnd/reset", "mdi:delete-restore", false);
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_button_entity("WiFi Reconnect", "energy_meter/cmnd/wifi_reconnect", "mdi:wifi-refresh");
    vTaskDelay(pdMS_TO_TICKS(10));
    publish_button_entity("Republish Discovery", "energy_meter/cmnd/republish_discovery", "mdi:database-sync");
    vTaskDelay(pdMS_TO_TICKS(10));

    for (int i = 0; i < num_channels; ++i) {
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "Channel %d Active", i + 1);
        publish_switch_entity(name_buf, "energy_meter/cmnd/channel/" + std::to_string(i),
                              "energy_meter/stat/channel/" + std::to_string(i), "mdi:power");
        vTaskDelay(pdMS_TO_TICKS(10));

        snprintf(name_buf, sizeof(name_buf), "Channel %d Phase", i + 1);
        publish_select_entity(name_buf, "energy_meter/cmnd/phase/" + std::to_string(i),
                              "energy_meter/stat/phase/" + std::to_string(i),
                              "L1,L2,L3,None", "mdi:alpha-p-circle");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Finished publishing control entities.");
}

void HaDiscovery::publish_button_entity(const std::string& name, const std::string& command_topic,
                                        const std::string& icon, bool enabled_by_default) {
    char topic[128];
    std::string sanitized_name = name;
    for (auto& c : sanitized_name) {
        if (c == ' ')
            c = '_';
    }
    snprintf(topic, sizeof(topic), "homeassistant/button/%s_%s/config", _device_id.c_str(),
             sanitized_name.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"" << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"command_topic\": \"" << command_topic << "\",";
    payload << "\"payload_press\": \"PRESS\",";
    payload << "\"icon\": \"" << icon << "\",";
    payload << "\"entity_category\": \"config\",";
    payload << "\"enabled_by_default\": " << (enabled_by_default ? "true" : "false") << ",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";

    append_main_device_json(payload);
    payload << "}";

    _mqtt_manager.publish(topic, payload.str(), 1, true);
}

void HaDiscovery::publish_number_entity(const std::string& name, const std::string& command_topic,
                                        const std::string& state_topic, int min, int max, int step,
                                        const std::string& unit, const std::string& icon) {
    char topic[128];
    std::string sanitized_name = name;
    for (auto& c : sanitized_name) {
        if (c == ' ')
            c = '_';
    }
    snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s/config", _device_id.c_str(),
             sanitized_name.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"" << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"command_topic\": \"" << command_topic << "\",";
    payload << "\"state_topic\": \"" << state_topic << "\",";
    payload << "\"min\": " << min << ",";
    payload << "\"max\": " << max << ",";
    payload << "\"step\": " << step << ",";
    payload << "\"unit_of_measurement\": \"" << unit << "\",";
    payload << "\"icon\": \"" << icon << "\",";
    payload << "\"entity_category\": \"config\",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";

    append_main_device_json(payload);
    payload << "}";

    _mqtt_manager.publish(topic, payload.str(), 1, true);
}

void HaDiscovery::publish_switch_entity(const std::string& name, const std::string& command_topic,
                                        const std::string& state_topic, const std::string& icon) {
    char topic[128];
    std::string sanitized_name = name;
    for (auto& c : sanitized_name) {
        if (c == ' ')
            c = '_';
    }
    snprintf(topic, sizeof(topic), "homeassistant/switch/%s_%s/config", _device_id.c_str(),
             sanitized_name.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"" << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"command_topic\": \"" << command_topic << "\",";
    payload << "\"state_topic\": \"" << state_topic << "\",";
    payload << "\"payload_on\": \"ON\",";
    payload << "\"payload_off\": \"OFF\",";
    payload << "\"state_on\": \"ON\",";
    payload << "\"state_off\": \"OFF\",";
    payload << "\"icon\": \"" << icon << "\",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";

    append_main_device_json(payload);
    payload << "}";

    _mqtt_manager.publish(topic, payload.str(), 1, true);
}

void HaDiscovery::publish_select_entity(const std::string& name, const std::string& command_topic,
                                        const std::string& state_topic, const std::string& options,
                                        const std::string& icon) {
    char topic[128];
    std::string sanitized_name = name;
    for (auto& c : sanitized_name) {
        if (c == ' ')
            c = '_';
    }
    snprintf(topic, sizeof(topic), "homeassistant/select/%s_%s/config", _device_id.c_str(),
             sanitized_name.c_str());

    std::ostringstream payload;
    payload << "{";
    payload << "\"name\": \"" << name << "\",";
    payload << "\"object_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"unique_id\": \"" << _device_id << "_" << sanitized_name << "\",";
    payload << "\"command_topic\": \"" << command_topic << "\",";
    payload << "\"state_topic\": \"" << state_topic << "\",";
    payload << "\"options\": [";

    size_t start = 0;
    size_t end = options.find(',');
    bool first = true;
    while (end != std::string::npos) {
        if (!first)
            payload << ",";
        payload << "\"" << options.substr(start, end - start) << "\"";
        first = false;
        start = end + 1;
        end = options.find(',', start);
    }
    if (!first)
        payload << ",";
    payload << "\"" << options.substr(start) << "\"";

    payload << "],";
    payload << "\"icon\": \"" << icon << "\",";
    payload << "\"entity_category\": \"config\",";
    payload << "\"availability_topic\": \"" << _status_topic << "\",";
    payload << "\"availability_template\": \"{{ value_json.status }}\",";
    payload << "\"payload_available\": \"alive\",";
    payload << "\"payload_not_available\": \"dead\",";

    append_main_device_json(payload);
    payload << "}";

    _mqtt_manager.publish(topic, payload.str(), 1, true);
}