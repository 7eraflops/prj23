#include "command_handler.hpp"

#include <esp_log.h>
#include <esp_system.h>
#include <cstdlib>
#include <cstring>

static const char* TAG = "CMD_HANDLER";

CommandHandler::CommandHandler(MqttManager& mqtt, ConfigManager& config, WifiManager& wifi,
                               const std::string& device_id)
    : _mqtt(mqtt), _config(config), _wifi(wifi), _device_id(device_id),
      _cmd_queue(nullptr), _task_handle(nullptr) {}

CommandHandler::~CommandHandler() {
    if (_task_handle) {
        vTaskDelete(_task_handle);
    }
    if (_cmd_queue) {
        vQueueDelete(_cmd_queue);
    }
}

void CommandHandler::start() {
    ESP_LOGI(TAG, "Subscribing to command topics...");
    _mqtt.subscribe("energy_meter/cmnd/#", 1);

    _cmd_queue = xQueueCreate(CMD_QUEUE_SIZE, sizeof(MqttCommand));
    if (!_cmd_queue) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return;
    }

    xTaskCreate(task_loop, "cmd_handler", 8192, this, 1, &_task_handle);

    ESP_LOGI(TAG, "Publishing initial states...");
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        publish_channel_state(i);
        publish_phase_state(i);
    }

    ESP_LOGI(TAG, "Command handler started.");
}

void CommandHandler::on_message(const std::string& topic, const std::string& payload) {
    ESP_LOGI(TAG, "Received command: topic=%s payload=%s", topic.c_str(), payload.c_str());

    if (!_cmd_queue) {
        ESP_LOGW(TAG, "Command queue not initialized");
        return;
    }

    MqttCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    strncpy(cmd.topic, topic.c_str(), CMD_TOPIC_MAX_LEN - 1);
    strncpy(cmd.payload, payload.c_str(), CMD_PAYLOAD_MAX_LEN - 1);

    if (xQueueSend(_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "Command queue full, dropping command");
    }
}

void CommandHandler::task_loop(void* arg) {
    CommandHandler* self = static_cast<CommandHandler*>(arg);
    MqttCommand cmd;

    while (true) {
        if (xQueueReceive(self->_cmd_queue, &cmd, portMAX_DELAY) == pdPASS) {
            self->process_command(cmd);
        }
    }
}

void CommandHandler::process_command(const MqttCommand& cmd) {
    const std::string prefix = "energy_meter/cmnd/";
    std::string topic(cmd.topic);

    if (topic.rfind(prefix, 0) != 0) {
        return;
    }

    std::string action = topic.substr(prefix.size());
    std::string payload(cmd.payload);

    if (action == "reboot") {
        handle_reboot();
    } else if (action == "reset") {
        handle_factory_reset();
    } else if (action == "wifi_reconnect") {
        handle_wifi_reconnect();
    } else if (action == "republish_discovery") {
        if (_republish_cb) {
            ESP_LOGI(TAG, "Republish discovery command received.");
            _republish_cb();
        }
    } else if (action.rfind("channel/", 0) == 0) {
        int channel = atoi(action.c_str() + 8);
        if (channel >= 0 && channel < NUM_CHANNELS) {
            handle_channel(channel, payload);
        }
    } else if (action.rfind("phase/", 0) == 0) {
        int channel = atoi(action.c_str() + 6);
        if (channel >= 0 && channel < NUM_CHANNELS) {
            handle_phase(channel, payload);
        }
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", action.c_str());
    }
}

void CommandHandler::publish_status() {
    char buf[512];
    int offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - offset,
                       "{\"device_id\":\"%s\",",
                       _device_id.c_str());

    offset += snprintf(buf + offset, sizeof(buf) - offset, "\"channel_active\":");
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s%d", i == 0 ? "" : ",",
                           _config.is_channel_active(i) ? 1 : 0);
    }

    offset += snprintf(buf + offset, sizeof(buf) - offset, ",\"channel_phase\":[");
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s%d", i == 0 ? "" : ",",
                           static_cast<int>(_config.get_channel_phase(i)));
    }
    snprintf(buf + offset, sizeof(buf) - offset, "]}");

    _mqtt.publish("energy_meter/status", buf, 0, false);
}

void CommandHandler::handle_reboot() {
    ESP_LOGI(TAG, "Reboot command received. Shutting down gracefully...");
    _mqtt.stop();
    _wifi.set_mode(WIFI_MODE_NULL);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

void CommandHandler::handle_factory_reset() {
    ESP_LOGW(TAG, "Factory reset command received. Clearing config and rebooting...");
    _mqtt.publish("energy_meter/stat/reset", "factory_reset", 0, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    _config.clear();
    esp_restart();
}

void CommandHandler::handle_wifi_reconnect() {
    ESP_LOGI(TAG, "WiFi reconnect command received.");
    const AppConfig& cfg = _config.get_config();
    _wifi.set_mode(WIFI_MODE_STA);
    _wifi.connect(cfg.wifi_ssid, cfg.wifi_password);
    _mqtt.publish("energy_meter/stat/wifi_reconnect", "triggered", 0, false);

    vTaskDelay(pdMS_TO_TICKS(2000));
    if (_mqtt.is_connected()) {
        ESP_LOGI(TAG, "MQTT still connected after WiFi reconnect, republishing states...");
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            publish_channel_state(i);
            publish_phase_state(i);
        }
    }
}

void CommandHandler::handle_channel(int channel, const std::string& payload) {
    bool active = (payload == "ON" || payload == "on" || payload == "1" || payload == "true");
    _config.set_channel_active(channel, active);
    publish_channel_state(channel);
    ESP_LOGI(TAG, "Channel %d active=%s", channel, active ? "true" : "false");

    _config.save_channel_settings();
}

void CommandHandler::handle_phase(int channel, const std::string& payload) {
    ChannelPhase phase;
    if (payload == "Phase A" || payload == "A" || payload == "1") {
        phase = ChannelPhase::PHASE_A;
    } else if (payload == "Phase B" || payload == "B" || payload == "2") {
        phase = ChannelPhase::PHASE_B;
    } else if (payload == "Phase C" || payload == "C" || payload == "3") {
        phase = ChannelPhase::PHASE_C;
    } else {
        phase = ChannelPhase::NONE;
    }

    _config.set_channel_phase(channel, phase);
    publish_phase_state(channel);
    ESP_LOGI(TAG, "Channel %d phase=%d", channel, static_cast<int>(phase));

    _config.save_channel_settings();
}

void CommandHandler::publish_channel_state(int channel) {
    char topic[64];
    snprintf(topic, sizeof(topic), "energy_meter/stat/channel/%d", channel);
    _mqtt.publish(topic, _config.is_channel_active(channel) ? "ON" : "OFF", 0, true);
}

void CommandHandler::publish_phase_state(int channel) {
    char topic[64];
    snprintf(topic, sizeof(topic), "energy_meter/stat/phase/%d", channel);

    ChannelPhase phase = _config.get_channel_phase(channel);
    const char* phase_str;
    switch (phase) {
    case ChannelPhase::PHASE_A:
        phase_str = "Phase A";
        break;
    case ChannelPhase::PHASE_B:
        phase_str = "Phase B";
        break;
    case ChannelPhase::PHASE_C:
        phase_str = "Phase C";
        break;
    default:
        phase_str = "None";
        break;
    }

    _mqtt.publish(topic, phase_str, 0, true);
}

void CommandHandler::republish_all_states() {
    ESP_LOGI(TAG, "Republishing all states...");
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        publish_channel_state(i);
        publish_phase_state(i);
    }
    publish_status();
}

void CommandHandler::on_republish_discovery(RepublishCallback cb) {
    _republish_cb = std::move(cb);
}
