#pragma once

#include <esp_err.h>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

inline constexpr uint16_t NUM_CHANNELS = 12;

enum class ChannelPhase : uint8_t {
    NONE = 0,
    PHASE_L1 = 1,
    PHASE_L2 = 2,
    PHASE_L3 = 3
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

    struct ChannelCalibration {
        float energy_offset_kwh = 0.0F;
    };

    ChannelCalibration channel_calibration[NUM_CHANNELS];

    struct CalibrationData {
        uint16_t pl_const_h = 0x0861;
        uint16_t pl_const_l = 0xC468;
        bool line_freq_50hz = true;
        uint8_t pga_gain_mode = 0;

        uint16_t p_start_th = 0x0000;
        uint16_t q_start_th = 0x0000;
        uint16_t s_start_th = 0x0000;
        uint16_t p_phase_th = 0x0000;
        uint16_t q_phase_th = 0x0000;
        uint16_t s_phase_th = 0x0000;

        int16_t p_offset[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int16_t q_offset[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint16_t pq_gain[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int16_t phi[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        uint16_t u_gain[3]   = {0xC7CE, 0xC7CE, 0xC7CE};
        int16_t u_offset[3] = {0, 0, 0};
        uint16_t i_gain[12]  = {0x27A4, 0x27A4, 0x27A4, 0x27A4,
                                 0x27A4, 0x27A4, 0x27A4, 0x27A4,
                                 0x27A4, 0x27A4, 0x27A4, 0x27A4};
        int16_t i_offset[12]= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    };

    CalibrationData calibration_data;

    struct EnergyTotals {
        double total_kwh[NUM_CHANNELS] = {0.0};
    };

    EnergyTotals energy_totals;

    AppConfig() {
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            if (i < 4) {
                channel_phases[i] = ChannelPhase::PHASE_L1;
            } else if (i < 8) {
                channel_phases[i] = ChannelPhase::PHASE_L2;
            } else {
                channel_phases[i] = ChannelPhase::PHASE_L3;
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
    AppConfig get_config() const;

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

    AppConfig::ChannelCalibration get_channel_calibration(int channel) const;
    void set_channel_calibration(int channel, const AppConfig::ChannelCalibration& calibration);
    void save_calibration_settings();

    AppConfig::CalibrationData get_calibration_data() const;
    void set_calibration_data(const AppConfig::CalibrationData& data);
    void save_hardware_calibration();

    AppConfig::EnergyTotals get_energy_totals() const;
    void set_energy_totals(const AppConfig::EnergyTotals& totals);
    void save_energy_totals();

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

    esp_err_t load_blob(const char* key, void* value, size_t max_size, size_t* actual_size = nullptr);
    esp_err_t save_blob(const char* key, const void* value, size_t value_size);

    void load_channel_settings();
    void load_calibration_settings();
    void load_hardware_calibration();
    void load_energy_totals();
};
