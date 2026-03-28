#pragma once

#include <cstddef>
#include <esp_err.h>
#include <esp_ota_ops.h>

namespace ota_manager {

/**
 * @brief Manages the OTA (Over-the-Air) firmware update process.
 */
class OtaManager {
public:
    OtaManager();
    ~OtaManager();

    /**
     * @brief Prepares the device for a new firmware update.
     *
     * Finds the next available OTA partition and begins the update process.
     *
     * @return esp_err_t ESP_OK on success, or an error code on failure.
     */
    esp_err_t begin();

    /**
     * @brief Writes a chunk of firmware data to the flash memory.
     *
     * @param data Pointer to the binary data chunk.
     * @param length Size of the data chunk in bytes.
     * @return esp_err_t ESP_OK on success, or an error code on failure.
     */
    esp_err_t write(const void* data, size_t length);

    /**
     * @brief Finalizes the firmware update.
     *
     * Validates the written data and sets the new partition as the boot target.
     *
     * @return esp_err_t ESP_OK on success, or an error code on failure.
     */
    esp_err_t end();

    /**
     * @brief Aborts the current update process and cleans up resources.
     */
    void abort();

private:
    esp_ota_handle_t _update_handle = 0;
    const esp_partition_t* _update_partition = nullptr;
    bool _is_ongoing = false;
};

} // namespace ota_manager
