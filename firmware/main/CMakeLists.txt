idf_component_register(SRCS "src/main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_manager config_manager setup_server board_manager mdns_manager mqtt_manager ha_discovery)
