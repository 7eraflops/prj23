#pragma once

#include "config_manager.hpp"
#include "mqtt_client.h"

#include <esp_err.h>
#include <functional>
#include <mutex>
#include <string>

class MqttManager {
public:
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;
    using MessageCallback =
        std::function<void(const std::string& topic, const std::string& payload)>;

    /**
     * @brief Construct a new Mqtt Manager object
     *
     * @param config_manager Reference to the config manager to retrieve MQTT credentials
     */
    explicit MqttManager(const ConfigManager& config_manager);

    /**
     * @brief Destroy the Mqtt Manager object
     */
    ~MqttManager();

    /**
     * @brief Start the MQTT client and attempt to connect
     *
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t start();

    /**
     * @brief Stop the MQTT client and disconnect
     *
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t stop();

    /**
     * @brief Check if currently connected to the MQTT broker
     *
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

    /**
     * @brief Publish a message to a topic
     *
     * @param topic The MQTT topic
     * @param payload The message payload
     * @param qos Quality of Service (0, 1, or 2)
     * @param retain Retain flag
     * @return int message_id of the publish message, or -1 on failure
     */
    int publish(const std::string& topic, const std::string& payload, int qos = 0,
                bool retain = false);

    /**
     * @brief Subscribe to an MQTT topic
     *
     * @param topic The MQTT topic to subscribe to
     * @param qos Quality of Service (0, 1, or 2)
     * @return int message_id of the subscribe message, or -1 on failure
     */
    int subscribe(const std::string& topic, int qos = 0);

    /**
     * @brief Register a callback for when the client connects to the broker
     */
    void on_connect(ConnectCallback cb);

    /**
     * @brief Register a callback for when the client disconnects from the broker
     */
    void on_disconnect(DisconnectCallback cb);

    /**
     * @brief Register a callback for incoming messages (catch-all)
     */
    void on_message(MessageCallback cb);

private:
    const ConfigManager& _config_manager;
    esp_mqtt_client_handle_t _client;
    bool _connected;
    mutable std::mutex _mtx;

    ConnectCallback _connect_cb;
    DisconnectCallback _disconnect_cb;
    MessageCallback _message_cb;

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                                   void* event_data);
    void handle_event(esp_mqtt_event_handle_t event);
};
