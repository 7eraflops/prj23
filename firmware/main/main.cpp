#include "wifi_manager.hpp"
#include <esp_log.h>
#include <esp_mac.h>
#include <iomanip>
#include <sstream>

static const char* TAG = "Main";

extern "C" void app_main() {
    ESP_LOGI(TAG, "Starting Energy Meter project");

    WifiManager wifi;

    // Initialize WiFi and NVS
    if (wifi.init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi Manager");
        return;
    }

    // En-Meter indicates Energy Meter. 
    std::string ssid_prefix = "En-Meter";

    // Generate a unique Proof of Possession (POP) based on the device's MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    std::stringstream pop_stream;
    pop_stream << std::hex << std::setfill('0');
    pop_stream << "m3t3r_"; // Prefix for the POP
    for (int i = 3; i < 6; i++) { // Use the last 3 bytes of the MAC address
        pop_stream << std::setw(2) << static_cast<int>(mac[i]);
    }
    std::string pop = pop_stream.str();
    
    ESP_LOGI(TAG, "Device POP configured as: %s", pop.c_str());

    if (wifi.start_provisioning(ssid_prefix, pop) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning");
        return;
    }

    // Wait for connection to be established
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    wifi.wait_for_connection();

    ESP_LOGI(TAG, "WiFi Connected! Starting main application logic...");

    // TODO: Add Energy Meter logic here
    while (true) {
        ESP_LOGI(TAG, "Energy Meter heartbeat...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
