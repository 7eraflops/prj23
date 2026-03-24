#pragma once

#include <esp_err.h>

class WifiManager;
class ConfigManager;

namespace setup_server {

/**
 * @brief Initialize and start the setup server (HTTP Web server)
 *
 * @param wifi Reference to the WifiManager for scanning networks
 * @param config Reference to the ConfigManager for saving credentials
 * @return esp_err_t ESP_OK on success
 */
esp_err_t start(WifiManager& wifi, ConfigManager& config);

/**
 * @brief Stop the setup server
 */
void stop();

} // namespace setup_server
