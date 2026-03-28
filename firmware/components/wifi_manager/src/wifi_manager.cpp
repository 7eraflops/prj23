#include "wifi_manager.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <cstring>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

static const char* TAG = "WifiManager";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

WifiManager::WifiManager() {}

WifiManager::~WifiManager() {
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
    }
}

esp_err_t WifiManager::init() {
    // Initialize NVS (needed for Wi-Fi core)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default network interfaces for both AP and STA
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Event Handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &WifiManager::event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &WifiManager::event_handler, this));

    return ESP_OK;
}

esp_err_t WifiManager::connect(const std::string& ssid, const std::string& password) {
    ESP_LOGI(TAG, "Starting Station mode. Connecting to: %s", ssid.c_str());

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password.c_str(),
            sizeof(wifi_config.sta.password) - 1);

    if (password.empty()) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    // Default to APSTA if no mode is explicitly set to allow concurrent scan/setup,
    // but the caller can call set_mode(WIFI_MODE_STA) before connect() to disable AP.
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) != ESP_OK || current_mode == WIFI_MODE_NULL) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    return ESP_OK;
}

esp_err_t WifiManager::start_ap(const std::string& ap_ssid) {
    ESP_LOGI(TAG, "Starting AP mode. SSID: %s", ap_ssid.c_str());

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ap_ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = ap_ssid.length();
    wifi_config.ap.channel = 1;
    wifi_config.ap.password[0] = '\0'; // Open network
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    // Use APSTA mode so we can broadcast the setup network AND scan for routers simultaneously
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

std::vector<WifiNetwork> WifiManager::scan_networks() {
    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    std::vector<WifiNetwork> networks;

    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = false;

    // Perform a blocking scan
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(ret));
        return networks;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No Wi-Fi networks found");
        return networks;
    }

    // Allocate memory to hold scan results
    wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    // Convert ESP-IDF records to our clean C++ struct
    for (int i = 0; i < ap_count; i++) {
        WifiNetwork net;
        net.ssid = std::string((char*)ap_records[i].ssid);
        net.rssi = ap_records[i].rssi;
        net.requires_password = (ap_records[i].authmode != WIFI_AUTH_OPEN);

        // Filter out empty SSIDs
        if (!net.ssid.empty()) {
            networks.push_back(net);
        }
    }

    delete[] ap_records;
    ESP_LOGI(TAG, "Scan complete, found %d networks", networks.size());

    return networks;
}

void WifiManager::wait_for_connection() {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

esp_err_t WifiManager::clear_settings() {
    ESP_LOGI(TAG, "Restoring Wi-Fi stack to defaults...");
    return esp_wifi_restore();
}

esp_err_t WifiManager::set_mode(wifi_mode_t mode) {
    ESP_LOGI(TAG, "Setting Wi-Fi mode to: %d", (int)mode);
    return esp_wifi_set_mode(mode);
}

std::string WifiManager::get_sta_ip() const {
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char buf[32];
        esp_ip4addr_ntoa(&ip_info.ip, buf, sizeof(buf));
        return std::string(buf);
    }
    return "0.0.0.0";
}

void WifiManager::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                void* event_data) {
    WifiManager* self = static_cast<WifiManager*>(arg);
    if (event_base == WIFI_EVENT) {
        self->on_wifi_event(event_id, event_data);
    } else if (event_base == IP_EVENT) {
        self->on_ip_event(event_id, event_data);
    }
}

void WifiManager::on_wifi_event(int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        // We handle connecting explicitly in connect() or it's handled by auto-reconnect
    } else if (event_id == WIFI_EVENT_AP_START) {
        // AP started
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from WiFi, retrying...");
        _is_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        // Auto-reconnect if we are in STA mode
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            esp_wifi_connect();
        }
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "Device connected to our AP: " MACSTR, MAC2STR(event->mac));
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "Device disconnected from our AP: " MACSTR, MAC2STR(event->mac));
    }
}

void WifiManager::on_ip_event(int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        _is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}