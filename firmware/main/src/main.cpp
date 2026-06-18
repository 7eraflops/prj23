#include "board_manager.hpp"
#include "command_handler.hpp"
#include "config_manager.hpp"
#include "ha_discovery.hpp"
#include "mdns_manager.hpp"
#include "mqtt_manager.hpp"
#include "sensor_manager.hpp"
#include "atm90e32_sensor.hpp"
#include "web_server.hpp"
#include "wifi_manager.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>

static const char* TAG = "Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting Energy Meter project");

    static ConfigManager config_mgr;
    static WifiManager wifi;
    static MqttManager mqtt(config_mgr);

    uint8_t base_mac[6];
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "energymeter_%02x%02x%02x", base_mac[3], base_mac[4],
             base_mac[5]);
    static HaDiscovery ha_discovery(mqtt, std::string(device_id));
    static CommandHandler cmd_handler(mqtt, config_mgr, wifi, std::string(device_id));
    static auto sensor_backend = std::make_unique<ATM90E32Sensor>(config_mgr);
    static SensorManager sensor_manager(std::move(sensor_backend), mqtt, NUM_CHANNELS);

    board_manager::init_temperature_sensor();

    // Initialize reset button (GPIO 0 - BOOT button on DevKit)
    // Holding for 5 seconds will clear credentials and reboot
    board_manager::init_reset_button(GPIO_NUM_0, 5000, [&]() {
        ESP_LOGW(TAG, "Reset button held. Clearing configuration and rebooting...");
        mqtt.stop();
        wifi.set_mode(WIFI_MODE_NULL);
        config_mgr.clear();
        wifi.clear_settings();
        vTaskDelay(pdMS_TO_TICKS(200));
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

    // Always start the Web Server for local management
    if (web_server::start(wifi, config_mgr) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Web Server");
    }

    if (config_mgr.is_configured()) {
        ESP_LOGI(TAG, "Device is configured. Connecting to Wi-Fi...");

        const AppConfig& cfg = config_mgr.get_config();

        // Turn off AP mode if we are already configured
        wifi.set_mode(WIFI_MODE_STA);
        wifi.connect(cfg.wifi_ssid, cfg.wifi_password);

        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        wifi.wait_for_connection();
        ESP_LOGI(TAG, "WiFi Connected!");

        mqtt.on_message([&](const std::string& topic, const std::string& payload) {
            cmd_handler.on_message(topic, payload);
        });

        mqtt.on_connect([&]() {
            ESP_LOGI(TAG, "MQTT Connected! Publishing HA Auto-Discovery...");
            ha_discovery.publish_discovery_messages(12);
            cmd_handler.start();
            cmd_handler.publish_status();
            cmd_handler.republish_all_states();
        });

        cmd_handler.on_republish_discovery([&]() {
            ha_discovery.publish_discovery_messages(12);
        });

        if (mqtt.start() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start MQTT Manager");
        }

        if (!sensor_manager.start()) {
            ESP_LOGE(TAG, "Failed to start Sensor Manager");
        }
    } else {
        ESP_LOGI(TAG, "Device is NOT configured. Starting Provisioning AP...");

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char ssid_buf[32];
        snprintf(ssid_buf, sizeof(ssid_buf), "Energy Meter Setup %02X%02X%02X", mac[3], mac[4],
                 mac[5]);

        std::string ap_ssid(ssid_buf);
        if (wifi.start_ap(ap_ssid) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start Access Point");
            return;
        }

        ESP_LOGI(TAG, "AP Started. Waiting for users to connect to Web Server...");
    }
    while (true) {
        ESP_LOGI(TAG, "Energy Meter heartbeat...");
        if (mqtt.is_connected()) {
            wifi_ap_record_t ap_info;
            int rssi = 0;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                rssi = ap_info.rssi;
            }
            uint32_t free_heap = esp_get_free_heap_size();
            
            int64_t uptime_us = esp_timer_get_time();
            uint32_t total_seconds = static_cast<uint32_t>(uptime_us / 1000000ULL);
            uint32_t days = total_seconds / 86400;
            uint32_t hours = (total_seconds % 86400) / 3600;
            uint32_t minutes = (total_seconds % 3600) / 60;
            uint32_t seconds = total_seconds % 60;
            char uptime_str[32];
            snprintf(uptime_str, sizeof(uptime_str), "%lud %02luh %02lum %02lus", days, hours, minutes, seconds);

            float mcu_temp = board_manager::get_mcu_temperature();
            uint32_t stack_min = uxTaskGetStackHighWaterMark(NULL);

            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"status\":\"alive\",\"wifi_rssi\":%d,\"free_heap\":%lu,\"uptime\":%lu,\"uptime_str\":\"%s\","
                     "\"stack_min\":%lu,\"mcu_temp\":%.1f}",
                     rssi, (unsigned long)free_heap, (unsigned long)total_seconds, uptime_str,
                     (unsigned long)stack_min, mcu_temp);

            mqtt.publish("energy_meter/heartbeat", payload, 0, false);
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
