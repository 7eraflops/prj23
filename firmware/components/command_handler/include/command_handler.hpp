#pragma once

#include "config_manager.hpp"
#include "mqtt_manager.hpp"
#include "wifi_manager.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <functional>
#include <string>

static constexpr int CMD_TOPIC_MAX_LEN = 128;
static constexpr int CMD_PAYLOAD_MAX_LEN = 64;
static constexpr int CMD_QUEUE_SIZE = 16;

struct MqttCommand {
    char topic[CMD_TOPIC_MAX_LEN];
    char payload[CMD_PAYLOAD_MAX_LEN];
};

class CommandHandler {
public:
    using RepublishCallback = std::function<void()>;

    CommandHandler(MqttManager& mqtt, ConfigManager& config, WifiManager& wifi,
                   const std::string& device_id);

    ~CommandHandler();

    void start();

    void on_message(const std::string& topic, const std::string& payload);

    void publish_status();

    void republish_all_states();

    void on_republish_discovery(RepublishCallback cb);

private:
    MqttManager& _mqtt;
    ConfigManager& _config;
    WifiManager& _wifi;
    std::string _device_id;
    RepublishCallback _republish_cb;
    QueueHandle_t _cmd_queue;
    TaskHandle_t _task_handle;

    static void task_loop(void* arg);
    void process_command(const MqttCommand& cmd);

    void handle_reboot();
    void handle_factory_reset();
    void handle_wifi_reconnect();
    void handle_channel(int channel, const std::string& payload);
    void handle_phase(int channel, const std::string& payload);

    void publish_channel_state(int channel);
    void publish_phase_state(int channel);
};
