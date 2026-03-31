#pragma once

#include <esp_err.h>
#include <mutex>
#include <string>

static constexpr uint16_t NUM_CHANNELS = 12;

enum class ChannelPhase : uint8_t {
    NONE = 0,
    PHASE_A = 1,
    PHASE_B = 2,
    PHASE_C = 3
};

/**
 * @brief Holds all user-configurable parameters for the application.
 */
struct AppConfig {
    std::string wifi_ssid;
    std::string wifi_password;
    std::string mqtt_ip;
    std::string mqtt_username;
    std::string mqtt_password;

    uint16_t channel_active_mask = 0x0FFF;
    ChannelPhase channel_phases[NUM_CHANNELS];

    AppConfig() {
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            if (i < 4) {
                channel_phases[i] = ChannelPhase::PHASE_A;
            } else if (i < 8) {
                channel_phases[i] = ChannelPhase::PHASE_B;
            } else {
                channel_phases[i] = ChannelPhase::PHASE_C;
            }
        }
    }
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

    bool is_channel_active(int channel) const;
    void set_channel_active(int channel, bool active);

    ChannelPhase get_channel_phase(int channel) const;
    void set_channel_phase(int channel, ChannelPhase phase);

    void save_channel_settings();

private:
    AppConfig _config;
    mutable std::mutex _mtx;

    // Helper methods for NVS operations
    esp_err_t load_string(const char* key, std::string& value);
    esp_err_t save_string(const char* key, const std::string& value);

    esp_err_t load_u16(const char* key, uint16_t& value);
    esp_err_t save_u16(const char* key, uint16_t value);

    esp_err_t load_u8(const char* key, uint8_t& value);
    esp_err_t save_u8(const char* key, uint8_t value);

    void load_channel_settings();
};