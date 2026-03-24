#pragma once

#include <string>
#include <vector>
#include <map>
#include <esp_err.h>
#include <esp_event.h>

struct MqttBroker {
    std::string hostname;
    std::string ip;
    uint16_t port;
    std::map<std::string, std::string> txt;

    // Helper method to automatically deduce the protocol scheme
    std::string get_protocol() const {
        std::string scheme = "mqtt://"; // Default
        
        // Check if there's a TXT record specifying WebSockets
        auto it = txt.find("protocol");
        if (it != txt.end() && it->second == "ws") {
            scheme = "ws://";
        }

        return scheme;
    }
};

class MdnsManager {
public:
    /**
     * @brief Initialize the mDNS service with the given hostname.
     */
    static esp_err_t init(const std::string& hostname);

    /**
     * @brief Refresh or re-announce the mDNS services.
     */
    static void refresh_services();

    /**
     * @brief Discovers MQTT brokers on the local network using mDNS.
     * 
     * @return std::vector<MqttBroker> A list of discovered brokers.
     */
    static std::vector<MqttBroker> discover_mqtt_brokers();

    /**
     * @brief Core ESP-IDF event loop dispatcher for mDNS events (e.g., got IP, AP start).
     */
    static void event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
};