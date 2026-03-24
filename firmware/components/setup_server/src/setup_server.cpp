#include "setup_server.hpp"
#include "wifi_manager.hpp"
#include "config_manager.hpp"
#include <esp_log.h>
#include <esp_http_server.h>
#include "cJSON.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "mdns_manager.hpp"

static const char *TAG = "SetupServer";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

namespace setup_server {

static httpd_handle_t server = nullptr;
static WifiManager* g_wifi = nullptr;
static ConfigManager* g_config = nullptr;

static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    const size_t len = index_html_end - index_html_start;
    httpd_resp_send(req, (const char *)index_html_start, len);
    return ESP_OK;
}

static esp_err_t api_scan_get_handler(httpd_req_t *req) {
    if (!g_wifi) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    auto networks = g_wifi->scan_networks();
    
    cJSON *root = cJSON_CreateArray();
    for (const auto& net : networks) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", net.ssid.c_str());
        cJSON_AddNumberToObject(item, "rssi", net.rssi);
        cJSON_AddBoolToObject(item, "secure", net.requires_password);
        cJSON_AddItemToArray(root, item);
    }
    
    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static void reboot_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

static esp_err_t api_wifi_status_get_handler(httpd_req_t *req) {
    if (!g_wifi) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "connected", g_wifi->is_connected());
    cJSON_AddStringToObject(root, "sta_ip", g_wifi->get_sta_ip().c_str());
    
    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_mqtt_discovery_get_handler(httpd_req_t *req) {
    if (!g_wifi) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    auto brokers = MdnsManager::discover_mqtt_brokers();
    
    cJSON *root = cJSON_CreateArray();
    for (const auto& b : brokers) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "hostname", b.hostname.c_str());
        cJSON_AddStringToObject(item, "ip", b.ip.c_str());
        cJSON_AddNumberToObject(item, "port", b.port);
        cJSON_AddStringToObject(item, "protocol", b.get_protocol().c_str());
        
        cJSON *txt_obj = cJSON_CreateObject();
        for (const auto& [key, val] : b.txt) {
            cJSON_AddStringToObject(txt_obj, key.c_str(), val.c_str());
        }
        cJSON_AddItemToObject(item, "txt", txt_obj);
        
        cJSON_AddItemToArray(root, item);
    }
    
    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_reboot_post_handler(httpd_req_t *req) {
    httpd_resp_send(req, "{\"status\":\"rebooting\"}", -1);
    ESP_LOGI(TAG, "Reboot requested via API...");
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t api_save_post_handler(httpd_req_t *req) {
    if (!g_config) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int total_len = req->content_len;
    ESP_LOGI(TAG, "Incoming save payload size: %d bytes", total_len);
    if (total_len >= 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free(buf);
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    AppConfig cfg = g_config->get_config();

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    if (cJSON_IsString(ssid) && (ssid->valuestring != NULL)) {
        cfg.wifi_ssid = ssid->valuestring;
    }

    cJSON *password = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(password) && (password->valuestring != NULL)) {
        cfg.wifi_password = password->valuestring;
    }

    cJSON *mqtt_uri = cJSON_GetObjectItem(root, "mqtt_uri");
    if (cJSON_IsString(mqtt_uri) && (mqtt_uri->valuestring != NULL)) {
        cfg.mqtt_ip = mqtt_uri->valuestring;
    }

    cJSON *mqtt_user = cJSON_GetObjectItem(root, "mqtt_user");
    if (cJSON_IsString(mqtt_user) && (mqtt_user->valuestring != NULL)) {
        cfg.mqtt_username = mqtt_user->valuestring;
    }

    cJSON *mqtt_pass = cJSON_GetObjectItem(root, "mqtt_pass");
    if (cJSON_IsString(mqtt_pass) && (mqtt_pass->valuestring != NULL)) {
        cfg.mqtt_password = mqtt_pass->valuestring;
    }

    cJSON_Delete(root);

    g_config->set_config(cfg);
    g_config->save();

    // If wifi is changed, we try to connect immediately
    if (g_wifi) {
        g_wifi->connect(cfg.wifi_ssid, cfg.wifi_password);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", -1);

    return ESP_OK;
}

esp_err_t start(WifiManager& wifi, ConfigManager& config) {
    if (server) {
        return ESP_OK;
    }

    g_wifi = &wifi;
    g_config = &config;

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 10;
    server_config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting web server on port: '%d'", server_config.server_port);
    esp_err_t ret = httpd_start(&server, &server_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server!");
        return ret;
    }

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t scan_uri = {
        .uri       = "/api/scan",
        .method    = HTTP_GET,
        .handler   = api_scan_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &scan_uri);

    httpd_uri_t save_uri = {
        .uri       = "/api/save",
        .method    = HTTP_POST,
        .handler   = api_save_post_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &save_uri);

    httpd_uri_t status_uri = {
        .uri       = "/api/wifi-status",
        .method    = HTTP_GET,
        .handler   = api_wifi_status_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t mqtt_discovery_uri = {
        .uri       = "/api/mqtt-brokers",
        .method    = HTTP_GET,
        .handler   = api_mqtt_discovery_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &mqtt_discovery_uri);

    httpd_uri_t reboot_uri = {
        .uri       = "/api/reboot",
        .method    = HTTP_POST,
        .handler   = api_reboot_post_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &reboot_uri);

    return ESP_OK;
}

void stop() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
        ESP_LOGI(TAG, "Web server stopped");
    }
    g_wifi = nullptr;
    g_config = nullptr;
}

} // namespace setup_server