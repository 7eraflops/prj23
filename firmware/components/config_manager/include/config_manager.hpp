#pragma once

#include <string>
#include <esp_err.h>

/**
 * @brief Holds all user-configurable parameters for the application.
 */
struct AppConfig {
    std::string wifi_ssid;
    std::string wifi_password;
    std::string mqtt_ip;
    std::string mqtt_username;
    std::string mqtt_password;
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    /**
     * @brief Loads the configuration from NVS flash into memory.
     * @return ESP_OK on success, or an appropriate esp_err_t if not found/failed.
     */
    esp_err_t load();

    /**
     * @brief Saves the current in-memory configuration to NVS flash.
     * @return ESP_OK on success, or an appropriate esp_err_t on failure.
     */
    esp_err_t save();

    /**
     * @brief Erases all saved configuration data from NVS flash.
     * @return ESP_OK on success.
     */
    esp_err_t clear();

    /**
     * @brief Checks if the minimum required configuration exists (e.g., Wi-Fi SSID is not empty).
     * @return true if configured, false otherwise.
     */
    bool is_configured() const;

    /**
     * @brief Gets a reference to the current in-memory configuration.
     */
    const AppConfig& get_config() const;

    /**
     * @brief Updates the in-memory configuration. 
     * @note You must call save() after this to persist the changes to flash.
     */
    void set_config(const AppConfig& config);

private:
    AppConfig _config;

    // Helper methods for NVS string operations
    esp_err_t load_string(const char* key, std::string& value);
    esp_err_t save_string(const char* key, const std::string& value);
};