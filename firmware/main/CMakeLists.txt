idf_component_register(
    SRCS "src/main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        wifi_manager
        config_manager
        web_server
        board_manager
        mdns_manager
        mqtt_manager
        ha_discovery
        sensor_manager
        command_handler
)
