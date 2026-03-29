idf_component_register(SRCS "src/board_manager.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_timer esp_driver_tsens)
