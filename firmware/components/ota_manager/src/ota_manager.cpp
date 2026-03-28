#include "ota_manager.hpp"

#include <esp_log.h>

static const char* TAG = "OtaManager";

namespace ota_manager {

OtaManager::OtaManager() {}

OtaManager::~OtaManager() {
    if (_is_ongoing) {
        abort();
    }
}

esp_err_t OtaManager::begin() {
    if (_is_ongoing) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    _update_partition = esp_ota_get_next_update_partition(NULL);
    if (_update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get next update partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA update on partition: %s", _update_partition->label);

    esp_err_t err = esp_ota_begin(_update_partition, OTA_SIZE_UNKNOWN, &_update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        return err;
    }

    _is_ongoing = true;
    return ESP_OK;
}

esp_err_t OtaManager::write(const void* data, size_t length) {
    if (!_is_ongoing) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_write(_update_handle, data, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        abort();
        return err;
    }

    return ESP_OK;
}

esp_err_t OtaManager::end() {
    if (!_is_ongoing) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_end(_update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        _is_ongoing = false; // Handle was invalidated by esp_ota_end
        return err;
    }

    err = esp_ota_set_boot_partition(_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        _is_ongoing = false;
        return err;
    }

    ESP_LOGI(TAG, "OTA update successful!");
    _is_ongoing = false;
    return ESP_OK;
}

void OtaManager::abort() {
    if (_is_ongoing) {
        esp_ota_abort(_update_handle);
        _is_ongoing = false;
        ESP_LOGI(TAG, "OTA update aborted");
    }
}

} // namespace ota_manager
