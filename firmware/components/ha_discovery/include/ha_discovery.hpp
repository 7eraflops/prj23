#pragma once

#include "mqtt_manager.hpp"

#include <string>

/**
 * @brief Handles Home Assistant MQTT Auto-Discovery configuration.
 */
class HaDiscovery {
public:
    /**
     * @brief Construct a new Ha Discovery object
     *
     * @param mqtt_manager Reference to the MqttManager used for publishing
     * @param device_id Unique identifier for the device (e.g. MAC address or chip ID)
     */
    HaDiscovery(MqttManager& mqtt_manager, const std::string& device_id);

    /**
     * @brief Publish discovery payloads for all channels and their respective sensors.
     * @note This should be called after a successful MQTT connection is established.
     *
     * @param num_channels Number of channels to register (default 12)
     */
    void publish_discovery_messages(int num_channels = 12);

private:
    MqttManager& _mqtt_manager;
    std::string _status_topic;
    std::string _device_id;

    /**
     * @brief Helper to publish a single sensor's discovery configuration.
     *
     * @param channel The channel index (0-11)
     * @param sensor_type The type of measurement (e.g., "voltage", "active_power")
     * @param name The human-readable name of the sensor
     * @param unit The unit of measurement (e.g., "V", "W")
     * @param device_class The Home Assistant device_class (e.g., "voltage", "power")
     * @param state_class The Home Assistant state_class (e.g., "measurement")
     */
    void publish_sensor_discovery(int channel, const std::string& sensor_type,
                                  const std::string& name, const std::string& unit,
                                  const std::string& device_class, const std::string& state_class);

    /**
     * @brief Helper to publish diagnostic sensors (e.g. Wi-Fi signal, uptime, free heap)
     */
    void publish_diagnostic_discovery(const std::string& sensor_type, const std::string& name,
                                      const std::string& unit, const std::string& device_class,
                                      const std::string& entity_category,
                                      const std::string& state_class = "measurement");

    /**
     * @brief Publish control entities (buttons, numbers, switches, selects) for device management.
     * @note These appear on the main "ESP32 Energy Meter" device.
     *
     * @param num_channels Number of channels to create switch/select entities for
     */
    void publish_control_entities(int num_channels = 12);

private:
    void publish_button_entity(const std::string& name, const std::string& command_topic,
                               const std::string& icon, bool enabled_by_default = true);

    void publish_number_entity(const std::string& name, const std::string& command_topic,
                               const std::string& state_topic, int min, int max, int step,
                               const std::string& unit, const std::string& icon);

    void publish_switch_entity(const std::string& name, const std::string& command_topic,
                               const std::string& state_topic, const std::string& icon);

    void publish_select_entity(const std::string& name, const std::string& command_topic,
                               const std::string& state_topic, const std::string& options,
                               const std::string& icon);
};