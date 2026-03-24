#include "config_manager.hpp"
#include <nvs.h>
#include <esp_log.h>

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

esp_err_t ConfigManager::load() {
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    
    // We don't check for errors here because it's normal for some keys 
    // to not exist yet (e.g., first boot). The strings will just remain empty.
    load_string("wifi_ssid", _config.wifi_ssid);
    load_string("wifi_pass", _config.wifi_password);
    load_string("mqtt_ip", _config.mqtt_ip);
    load_string("mqtt_user", _config.mqtt_username);
    load_string("mqtt_pass", _config.mqtt_password);
    
    return ESP_OK;
}

esp_err_t ConfigManager::save() {
    ESP_LOGI(TAG, "Saving configuration to NVS...");
    
    esp_err_t err;
    err = save_string("wifi_ssid", _config.wifi_ssid);
    if (err != ESP_OK) return err;
    
    err = save_string("wifi_pass", _config.wifi_password);
    if (err != ESP_OK) return err;
    
    err = save_string("mqtt_ip", _config.mqtt_ip);
    if (err != ESP_OK) return err;
    
    err = save_string("mqtt_user", _config.mqtt_username);
    if (err != ESP_OK) return err;
    
    err = save_string("mqtt_pass", _config.mqtt_password);
    return err;
}

esp_err_t ConfigManager::clear() {
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
    // We consider the device configured if it at least has a Wi-Fi SSID
    return !_config.wifi_ssid.empty();
}

const AppConfig& ConfigManager::get_config() const {
    return _config;
}

void ConfigManager::set_config(const AppConfig& config) {
    _config = config;
}