idf_component_register(
    SRCS
        "src/atm90e32_spi.cpp"
        "src/atm90e32_device.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_timer log
)
