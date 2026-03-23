idf_component_register(
    SRCS "wifi_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event wifi_provisioning esp_http_server
    PRIV_REQUIRES nvs_flash esp_wifi qrcode
)
