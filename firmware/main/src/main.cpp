#include "wifi_manager.hpp"
#include "config_manager.hpp"
#include "setup_server.hpp"
#include "board_manager.hpp"
#include "mdns_manager.hpp"
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>

static const char* TAG = "Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting Energy Meter project");

    static ConfigManager config_mgr;
    static WifiManager wifi;

    // Initialize reset button (GPIO 0 - BOOT button on DevKit)
    // Holding for 5 seconds will clear credentials and reboot
    board_manager::init_reset_button(GPIO_NUM_0, 5000, []() {
        ESP_LOGW(TAG, "Reset button held. Clearing configuration and rebooting...");
        config_mgr.clear();
        wifi.clear_settings();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    });

    // Initialize WiFi and NVS
    if (wifi.init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager");
        return;
    }

    // Initialize mDNS Service
    if (MdnsManager::init("energy-meter") != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize mDNS manager");
    }

    // Load saved configuration from flash memory
    config_mgr.load();

    if (config_mgr.is_configured()) {
        ESP_LOGI(TAG, "Device is configured. Connecting to Wi-Fi...");

        const AppConfig& cfg = config_mgr.get_config();
        wifi.connect(cfg.wifi_ssid, cfg.wifi_password);

        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        wifi.wait_for_connection();
        ESP_LOGI(TAG, "WiFi Connected!");

        // TODO: Initialize MQTT Manager here using cfg.mqtt_ip
    } else {
        ESP_LOGI(TAG, "Device is NOT configured. Starting Setup Server...");

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char ssid_buf[32];
        snprintf(ssid_buf, sizeof(ssid_buf), "Energy Meter Setup %02X%02X%02X", mac[3], mac[4], mac[5]);

        std::string ap_ssid(ssid_buf);
        if (wifi.start_ap(ap_ssid) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start Access Point");
            return;
        }

        ESP_LOGI(TAG, "AP Started. Waiting for users to connect to Setup Server...");

        if (setup_server::start(wifi, config_mgr) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start Setup Server");
        }
    }
    while (true) {
        ESP_LOGI(TAG, "Energy Meter heartbeat...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
