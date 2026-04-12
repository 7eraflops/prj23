idf_component_register(
    SRCS
        "src/sensor_manager.cpp"
        "src/atm90e32_sensor.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES log mqtt_manager config_manager atm90e32_interface
)
