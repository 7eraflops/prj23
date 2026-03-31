#include "mdns_manager.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <mdns.h>

static const char* TAG = "MdnsManager";

static bool s_services_registered = false;

esp_err_t MdnsManager::init(const std::string& hostname) {
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    mdns_hostname_set(hostname.c_str());
    mdns_instance_name_set("Energy Meter Device");

    // Set hostname for netifs as well
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif)
        esp_netif_set_hostname(sta_netif, hostname.c_str());

    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif)
        esp_netif_set_hostname(ap_netif, hostname.c_str());

    refresh_services();

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START,
                                               &MdnsManager::event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &MdnsManager::event_handler, nullptr));

    return ESP_OK;
}

void MdnsManager::refresh_services() {
    if (s_services_registered) {
        esp_err_t ret = mdns_service_remove("_http", "_tcp");
        if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to remove mDNS service: %s", esp_err_to_name(ret));
        }
    }
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_txt_item_set("_http", "_tcp", "version", "1.0");
    s_services_registered = true;

    ESP_LOGI(TAG, "mDNS refreshed");
}

std::vector<MqttBroker> MdnsManager::discover_mqtt_brokers() {
    std::vector<MqttBroker> brokers;

    ESP_LOGI(TAG, "Querying mDNS for _mqtt._tcp services...");
    mdns_result_t* results = nullptr;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 3000, 20, &results);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS query failed: %s", esp_err_to_name(err));
        return brokers;
    }

    if (!results) {
        ESP_LOGI(TAG, "No MQTT brokers found via mDNS");
        return brokers;
    }

    mdns_result_t* r = results;
    while (r) {
        MqttBroker broker;
        if (r->hostname) {
            broker.hostname = r->hostname;
        }

        if (r->addr) {
            char buf[64];
            if (r->addr->addr.type == ESP_IPADDR_TYPE_V4) {
                esp_ip4addr_ntoa(&r->addr->addr.u_addr.ip4, buf, sizeof(buf));
            } else {
                snprintf(buf, sizeof(buf), IPV6STR, IPV62STR(r->addr->addr.u_addr.ip6));
            }
            broker.ip = buf;
        }

        broker.port = r->port;

        // Extract TXT records
        for (size_t i = 0; i < r->txt_count; i++) {
            mdns_txt_item_t* txt = &r->txt[i];
            if (txt->key && txt->value) {
                broker.txt[txt->key] = txt->value;
            }
        }

        brokers.push_back(broker);
        r = r->next;
    }

    mdns_query_results_free(results);
    ESP_LOGI(TAG, "Found %d MQTT brokers", (int)brokers.size());
    return brokers;
}

void MdnsManager::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        refresh_services();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Refresh mDNS after getting IP
        vTaskDelay(pdMS_TO_TICKS(500));
        refresh_services();
    }
}