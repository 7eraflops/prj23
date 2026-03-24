idf_component_register(
    SRCS "src/wifi_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_wifi
    PRIV_REQUIRES nvs_flash esp_netif setup_server
)
