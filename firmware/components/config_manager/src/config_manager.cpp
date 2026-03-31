#include "config_manager.hpp"

#include <esp_log.h>
#include <nvs.h>

static const char* TAG = "ConfigManager";
static const char* NVS_NAMESPACE = "app_config";

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

    return ESP_OK;
}

esp_err_t ConfigManager::save() {
    std::lock_guard<std::mutex> lock(_mtx);
    ESP_LOGI(TAG, "Saving configuration to NVS...");

    esp_err_t err;
    err = save_string("wifi_ssid", _config.wifi_ssid);
    if (err != ESP_OK)
        return err;

    err = save_string("wifi_pass", _config.wifi_password);
    if (err != ESP_OK)
        return err;

    err = save_string("mqtt_ip", _config.mqtt_ip);
    if (err != ESP_OK)
        return err;

    err = save_string("mqtt_user", _config.mqtt_username);
    if (err != ESP_OK)
        return err;

    err = save_string("mqtt_pass", _config.mqtt_password);
    if (err != ESP_OK)
        return err;

    err = save_u16("ch_active", _config.channel_active_mask);
    if (err != ESP_OK)
        return err;

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "ch_phase_%d", i);
        err = save_u8(key, static_cast<uint8_t>(_config.channel_phases[i]));
        if (err != ESP_OK)
            return err;
    }

    return ESP_OK;
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

const AppConfig& ConfigManager::get_config() const {
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
