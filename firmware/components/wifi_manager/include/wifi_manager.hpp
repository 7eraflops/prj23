#pragma once

#include <esp_err.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <string>
#include <vector>
#include <atomic>
#include <esp_timer.h>

// Represents a discovered Wi-Fi network during a scan
struct WifiNetwork {
    std::string ssid;
    int rssi;
    bool requires_password;
};

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    /**
     * @brief Initializes the core Wi-Fi stack and network interfaces.
     *        Must be called before any other Wi-Fi functions.
     */
    esp_err_t init();

    /**
     * @brief Starts Station (STA) mode and connects to the specified router.
     *
     * @param ssid The SSID of the target network.
     * @param password The password for the network.
     */
    esp_err_t connect(const std::string& ssid, const std::string& password);

    /**
     * @brief Starts Access Point (AP) mode to broadcast an open network.
     *        Typically used for the Captive Portal setup.
     *
     * @param ap_ssid The SSID that this device will broadcast.
     */
    esp_err_t start_ap(const std::string& ap_ssid);

    /**
     * @brief Scans the environment for available Wi-Fi networks.
     *        This is a blocking call until the scan completes.
     *
     * @return std::vector<WifiNetwork> A list of discovered networks.
     */
    std::vector<WifiNetwork> scan_networks();

    /**
     * @brief Blocks the current FreeRTOS task until a successful Wi-Fi connection is established.
     * @return true if connected successfully, false if timed out.
     */
    bool wait_for_connection();

    /**
     * @brief Erases saved Wi-Fi credentials from the internal ESP-IDF storage.
     * @return ESP_OK on success.
     */
    esp_err_t clear_settings();

    /**
     * @brief Sets the Wi-Fi mode (STA, AP, or APSTA).
     * @param mode The desired wifi_mode_t.
     * @return ESP_OK on success.
     */
    esp_err_t set_mode(wifi_mode_t mode);

    /**
     * @brief Returns true if the device is currently connected to Wi-Fi.
     */
    bool is_connected() const {
        return _is_connected;
    }

    /**
     * @brief Returns the current Station IP address as a string.
     */
    std::string get_sta_ip() const;

private:
    // Core ESP-IDF event loop dispatcher
    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                              void* event_data);

    // Specific event handlers
    void on_wifi_event(int32_t event_id, void* event_data);
    void on_ip_event(int32_t event_id, void* event_data);

    static void reconnect_timer_cb(void* arg);

    std::atomic<bool> _is_connected{false};
    esp_timer_handle_t _reconnect_timer = nullptr;
    int _reconnect_attempts = 0;
};
