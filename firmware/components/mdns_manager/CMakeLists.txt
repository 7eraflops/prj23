idf_component_register(
    SRCS "src/mdns_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mdns esp_netif esp_wifi esp_event
)
