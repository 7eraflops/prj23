idf_component_register(
    SRCS "src/command_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager config_manager wifi_manager log esp_timer freertos
)
