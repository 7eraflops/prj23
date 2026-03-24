idf_component_register(
    SRCS "src/mqtt_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt config_manager log esp_timer
)
