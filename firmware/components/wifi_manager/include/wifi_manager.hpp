#pragma once
#include <string>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <wifi_provisioning/manager.h>

class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    /**
     * @brief Initializes NVS flash and the core Wi-Fi stack. Must be called first.
     */
    esp_err_t init();

    /**
     * @brief Starts the SoftAP provisioning process if the device is not yet provisioned.
     *        Generates and prints a QR code to the console for easy mobile connection.
     * 
     * @param ssid_prefix The prefix for the SoftAP SSID broadcast by the device.
     * @param pop The Proof of Possession (password) required to connect via the app.
     */
    esp_err_t start_provisioning(const std::string& ssid_prefix, const std::string& pop);
    
    /**
     * @brief Returns true if the device already has saved Wi-Fi credentials in NVS.
     */
    bool is_provisioned();

    /**
     * @brief Blocks the current FreeRTOS task until a successful Wi-Fi connection is established.
     */
    void wait_for_connection();

private:
    static void      event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    void             on_wifi_event(int32_t event_id, void* event_data);
    void             on_ip_event(int32_t event_id, void* event_data);
    void             on_prov_event(int32_t event_id, void* event_data);

    static esp_err_t root_get_handler(httpd_req_t* req);
    void             print_qr_code(const std::string& ssid, const std::string& pop);

    bool           _is_connected    = false;
    bool           _is_provisioned  = false;
    bool           _is_provisioning = false;
    httpd_handle_t _prov_httpd      = nullptr;
};
