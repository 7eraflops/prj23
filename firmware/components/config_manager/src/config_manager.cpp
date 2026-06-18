#include "config_manager.hpp"

#include <esp_log.h>
#include <nvs.h>

#include <cstring>

static const char* TAG = "ConfigManager";
static const char* NVS_NAMESPACE = "app_config";
static constexpr uint8_t HW_CAL_VERSION = 1;

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {}

esp_err_t ConfigManager::load_string(const char* key, std::string& value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_str(handle, key, nullptr, &required_size);
    if (err == ESP_OK) {
        char* buffer = new char[required_size];
        err = nvs_get_str(handle, key, buffer, &required_size);
        if (err == ESP_OK) {
            value = buffer;
        }
        delete[] buffer;
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::save_string(const char* key, const std::string& value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, key, value.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::load_u16(const char* key, uint16_t& value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_u16(handle, key, &value);
    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::save_u16(const char* key, uint16_t value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u16(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::load_u8(const char* key, uint8_t& value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_u8(handle, key, &value);
    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::save_u8(const char* key, uint8_t value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::load_blob(const char* key, void* value, size_t max_size, size_t* actual_size) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_blob(handle, key, nullptr, &required_size);
    if (err == ESP_OK && required_size > 0) {
        size_t copy_size = std::min(required_size, max_size);
        if (copy_size < required_size) {
            // Read into temp buffer and copy truncated
            std::vector<uint8_t> temp(required_size);
            err = nvs_get_blob(handle, key, temp.data(), &required_size);
            if (err == ESP_OK) {
                memcpy(value, temp.data(), copy_size);
            }
        } else {
            // Read directly
            err = nvs_get_blob(handle, key, value, &required_size);
        }
        if (actual_size && err == ESP_OK) {
            *actual_size = copy_size;
        }
    }
    nvs_close(handle);

    return err;
}

esp_err_t ConfigManager::save_blob(const char* key, const void* value, size_t value_size) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, key, value, value_size);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t ConfigManager::load() {
    std::lock_guard<std::mutex> lock(_mtx);
    ESP_LOGI(TAG, "Loading configuration from NVS...");

    // We don't check for errors here because it's normal for some keys
    // to not exist yet (e.g., first boot). The strings will just remain empty.
    load_string("wifi_ssid", _config.wifi_ssid);
    load_string("wifi_pass", _config.wifi_password);
    load_string("mqtt_ip", _config.mqtt_ip);
    load_string("mqtt_user", _config.mqtt_username);
    load_string("mqtt_pass", _config.mqtt_password);

    load_u16("ch_active", _config.channel_active_mask);
    load_channel_settings();
    load_calibration_settings();
    load_hardware_calibration();
    load_energy_totals();

    return ESP_OK;
}

esp_err_t ConfigManager::save() {
    std::lock_guard<std::mutex> lock(_mtx);
    ESP_LOGI(TAG, "Saving configuration to NVS...");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_set_str(handle, "wifi_ssid", _config.wifi_ssid.c_str());
    nvs_set_str(handle, "wifi_pass", _config.wifi_password.c_str());
    nvs_set_str(handle, "mqtt_ip", _config.mqtt_ip.c_str());
    nvs_set_str(handle, "mqtt_user", _config.mqtt_username.c_str());
    nvs_set_str(handle, "mqtt_pass", _config.mqtt_password.c_str());

    nvs_set_u16(handle, "ch_active", _config.channel_active_mask);

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_phase_%d", i);
        nvs_set_u8(handle, key, static_cast<uint8_t>(_config.channel_phases[i]));
    }

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_cal_%d", i);
        nvs_set_blob(handle, key, &_config.channel_calibration[i], sizeof(_config.channel_calibration[i]));
    }

    nvs_set_blob(handle, "hw_cal", &_config.calibration_data, sizeof(_config.calibration_data));
    nvs_set_u8(handle, "hw_cal_v", HW_CAL_VERSION);

    nvs_set_blob(handle, "energy", &_config.energy_totals, sizeof(_config.energy_totals));

    err = nvs_commit(handle);
    nvs_close(handle);

    return err;
}

esp_err_t ConfigManager::clear() {
    std::lock_guard<std::mutex> lock(_mtx);
    ESP_LOGI(TAG, "Clearing configuration from NVS...");
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    // Clear in-memory config as well
    _config = AppConfig{};

    return err;
}

bool ConfigManager::is_configured() const {
    std::lock_guard<std::mutex> lock(_mtx);
    // We consider the device configured if it at least has a Wi-Fi SSID
    return !_config.wifi_ssid.empty();
}

AppConfig ConfigManager::get_config() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _config;
}

void ConfigManager::set_config(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(_mtx);
    _config = config;
}

bool ConfigManager::is_channel_active(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS)
        return false;
    std::lock_guard<std::mutex> lock(_mtx);
    return (_config.channel_active_mask & (1 << channel)) != 0;
}

void ConfigManager::set_channel_active(int channel, bool active) {
    if (channel < 0 || channel >= NUM_CHANNELS)
        return;
    std::lock_guard<std::mutex> lock(_mtx);
    if (active) {
        _config.channel_active_mask |= (1 << channel);
    } else {
        _config.channel_active_mask &= ~(1 << channel);
    }
}

ChannelPhase ConfigManager::get_channel_phase(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS)
        return ChannelPhase::NONE;
    std::lock_guard<std::mutex> lock(_mtx);
    return _config.channel_phases[channel];
}

void ConfigManager::set_channel_phase(int channel, ChannelPhase phase) {
    if (channel < 0 || channel >= NUM_CHANNELS)
        return;
    std::lock_guard<std::mutex> lock(_mtx);
    _config.channel_phases[channel] = phase;
}

void ConfigManager::save_channel_settings() {
    std::lock_guard<std::mutex> lock(_mtx);
    save_u16("ch_active", _config.channel_active_mask);
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_phase_%d", i);
        save_u8(key, static_cast<uint8_t>(_config.channel_phases[i]));
    }
}

AppConfig::ChannelCalibration ConfigManager::get_channel_calibration(int channel) const {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return AppConfig::ChannelCalibration{};
    }

    std::lock_guard<std::mutex> lock(_mtx);
    return _config.channel_calibration[channel];
}

void ConfigManager::set_channel_calibration(int channel,
                                            const AppConfig::ChannelCalibration& calibration) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mtx);
    _config.channel_calibration[channel] = calibration;
}

void ConfigManager::save_calibration_settings() {
    std::lock_guard<std::mutex> lock(_mtx);
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_cal_%d", i);
        save_blob(key, &_config.channel_calibration[i], sizeof(_config.channel_calibration[i]));
    }
}

void ConfigManager::load_channel_settings() {
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_phase_%d", i);
        uint8_t val;
        if (load_u8(key, val) == ESP_OK) {
            _config.channel_phases[i] = static_cast<ChannelPhase>(val);
        }
    }
}

void ConfigManager::load_calibration_settings() {
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_cal_%d", i);

        AppConfig::ChannelCalibration calibration{};
        const esp_err_t err = load_blob(key, &calibration, sizeof(calibration));
        if (err == ESP_OK) {
            _config.channel_calibration[i] = calibration;
        }
    }
}

void ConfigManager::load_hardware_calibration() {
    AppConfig::CalibrationData data{};
    size_t actual_size = 0;
    const esp_err_t err = load_blob("hw_cal", &data, sizeof(data), &actual_size);
    if (err == ESP_OK) {
        if (actual_size == 120) {
            ESP_LOGI(TAG, "Migrating hardware calibration from v1 (120 bytes) to v2.");
            struct CalibrationDataV1 {
                uint16_t pl_const_h;
                uint16_t pl_const_l;
                bool line_freq_50hz;
                uint8_t pga_gain_mode;

                uint16_t p_start_th;
                uint16_t q_start_th;
                uint16_t s_start_th;
                uint16_t p_phase_th;
                uint16_t q_phase_th;
                uint16_t s_phase_th;

                int16_t p_offset[3];
                int16_t q_offset[3];
                uint16_t pq_gain[3];
                int16_t phi[12];

                uint16_t u_gain[3];
                int16_t u_offset[3];
                uint16_t i_gain[12];
                int16_t i_offset[12];
            } old_data;

            load_blob("hw_cal", &old_data, sizeof(old_data), nullptr);

            data.pl_const_h = old_data.pl_const_h;
            data.pl_const_l = old_data.pl_const_l;
            data.line_freq_50hz = old_data.line_freq_50hz;
            data.pga_gain_mode = old_data.pga_gain_mode;
            data.p_start_th = old_data.p_start_th;
            data.q_start_th = old_data.q_start_th;
            data.s_start_th = old_data.s_start_th;
            data.p_phase_th = old_data.p_phase_th;
            data.q_phase_th = old_data.q_phase_th;
            data.s_phase_th = old_data.s_phase_th;

            for(int i = 0; i < 3; ++i) {
                data.p_offset[i] = old_data.p_offset[i];
                data.q_offset[i] = old_data.q_offset[i];
                data.pq_gain[i] = old_data.pq_gain[i];
                data.u_gain[i] = old_data.u_gain[i];
                data.u_offset[i] = old_data.u_offset[i];
            }
            for(int i = 0; i < 12; ++i) {
                data.phi[i] = old_data.phi[i];
                data.i_gain[i] = old_data.i_gain[i];
                data.i_offset[i] = old_data.i_offset[i];
            }
            _config.calibration_data = data;
        } else {
            _config.calibration_data = data;
            if (actual_size < sizeof(data)) {
                ESP_LOGW(TAG, "Hardware calibration struct expanded (stored=%zu, expected=%zu). "
                              "New fields set to default values.",
                         actual_size, sizeof(data));
            } else if (actual_size > sizeof(data)) {
                ESP_LOGW(TAG, "Hardware calibration struct shrunk (stored=%zu, expected=%zu). "
                              "Data truncated.",
                         actual_size, sizeof(data));
            }
        }
    } else {
        ESP_LOGW(TAG, "Hardware calibration not found or invalid. Using defaults.");
    }
}

AppConfig::CalibrationData ConfigManager::get_calibration_data() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _config.calibration_data;
}

void ConfigManager::set_calibration_data(const AppConfig::CalibrationData& data) {
    std::lock_guard<std::mutex> lock(_mtx);
    _config.calibration_data = data;
}

void ConfigManager::save_hardware_calibration() {
    std::lock_guard<std::mutex> lock(_mtx);
    save_blob("hw_cal", &_config.calibration_data, sizeof(_config.calibration_data));
    save_u8("hw_cal_v", HW_CAL_VERSION);
}

void ConfigManager::load_energy_totals() {
    AppConfig::EnergyTotals data{};
    size_t actual_size = 0;
    const esp_err_t err = load_blob("energy", &data, sizeof(data), &actual_size);
    if (err == ESP_OK) {
        _config.energy_totals = data;
    }
}

AppConfig::EnergyTotals ConfigManager::get_energy_totals() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _config.energy_totals;
}

void ConfigManager::set_energy_totals(const AppConfig::EnergyTotals& totals) {
    std::lock_guard<std::mutex> lock(_mtx);
    _config.energy_totals = totals;
}

void ConfigManager::save_energy_totals() {
    std::lock_guard<std::mutex> lock(_mtx);
    save_blob("energy", &_config.energy_totals, sizeof(_config.energy_totals));
}
