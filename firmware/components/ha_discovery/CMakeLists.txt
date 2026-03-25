idf_component_register(
    SRCS "src/ha_discovery.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager log
)
