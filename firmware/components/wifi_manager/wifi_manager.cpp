#include "wifi_manager.hpp"
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>
#include <qrcode.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

static const char* TAG = "WifiManager";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

WifiManager::WifiManager() {}
WifiManager::~WifiManager() {}

esp_err_t WifiManager::init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &WifiManager::event_handler, this));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

bool WifiManager::is_provisioned() {
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    _is_provisioned = provisioned;
    return provisioned;
}

esp_err_t WifiManager::start_provisioning(const std::string& ssid_prefix, const std::string& pop) {
    wifi_prov_mgr_config_t config = {};
    config.scheme               = wifi_prov_scheme_softap;
    config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
    config.app_event_handler    = WIFI_PROV_EVENT_HANDLER_NONE;
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        _is_provisioning = true;
        ESP_LOGI(TAG, "Starting provisioning");

        httpd_config_t srv_cfg   = HTTPD_DEFAULT_CONFIG();
        srv_cfg.max_open_sockets = 5;
        srv_cfg.max_uri_handlers = 12;
        ESP_ERROR_CHECK(httpd_start(&_prov_httpd, &srv_cfg));

        static const httpd_uri_t root_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = WifiManager::root_get_handler,
            .user_ctx = nullptr
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(_prov_httpd, &root_uri));

        static const httpd_uri_t catchall_uri = {
            .uri      = "/*",
            .method   = HTTP_GET,
            .handler  = WifiManager::root_get_handler,
            .user_ctx = nullptr
        };
        ESP_ERROR_CHECK(httpd_register_uri_handler(_prov_httpd, &catchall_uri));

        wifi_prov_scheme_softap_set_httpd_handle(&_prov_httpd);

        char service_name[33];
        uint8_t eth_mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
        snprintf(service_name, sizeof(service_name), "%s_%02X%02X%02X",
                 ssid_prefix.c_str(), eth_mac[3], eth_mac[4], eth_mac[5]);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1, pop.c_str(), service_name, nullptr));

        print_qr_code(service_name, pop);
    } else {
        ESP_LOGI(TAG, "Already provisioned, connecting to WiFi");
        wifi_prov_mgr_deinit();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_connect();
    }

    return ESP_OK;
}

void WifiManager::wait_for_connection() {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

void WifiManager::event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    WifiManager* self = static_cast<WifiManager*>(arg);
    if (event_base == WIFI_EVENT) {
        self->on_wifi_event(event_id, event_data);
    } else if (event_base == IP_EVENT) {
        self->on_ip_event(event_id, event_data);
    } else if (event_base == WIFI_PROV_EVENT) {
        self->on_prov_event(event_id, event_data);
    }
}

void WifiManager::on_wifi_event(int32_t event_id, void* event_data) {
    if (_is_provisioning) {
        return;
    }
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from WiFi, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void WifiManager::on_ip_event(int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        _is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void WifiManager::on_prov_event(int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t* wifi_cfg = (wifi_sta_config_t*) event_data;
            ESP_LOGI(TAG, "Received credentials: SSID: %s", (char*) wifi_cfg->ssid);
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t* reason = (wifi_prov_sta_fail_reason_t*) event_data;
            ESP_LOGE(TAG, "Provisioning failed: %s",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Auth error" : "AP not found");
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_END:
            _is_provisioning = false;
            wifi_prov_mgr_deinit();
            if (_prov_httpd) {
                httpd_stop(_prov_httpd);
                _prov_httpd = nullptr;
            }
            break;
        default:
            break;
    }
}

esp_err_t WifiManager::root_get_handler(httpd_req_t* req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

void WifiManager::print_qr_code(const std::string& ssid, const std::string& pop) {
    // Construct the standard ESP-IDF provisioning payload for SoftAP in JSON format.
    // This allows the official Espressif provisioning apps (or Home Assistant) to scan and connect.
    std::string payload = "{\"ver\":\"v1\",\"name\":\"" + ssid
                        + "\",\"pop\":\"" + pop
                        + "\",\"transport\":\"softap\"}";

    ESP_LOGI(TAG, "Scan this QR code with the ESP SoftAP Provisioning app:");

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func         = esp_qrcode_print_console;
    cfg.max_qrcode_version   = 10;
    cfg.qrcode_ecc_level     = ESP_QRCODE_ECC_LOW;

    ESP_ERROR_CHECK(esp_qrcode_generate(&cfg, payload.c_str()));

    ESP_LOGI(TAG, "Alternatively, connect manually to SoftAP: %s with POP: %s",
             ssid.c_str(), pop.c_str());
}
