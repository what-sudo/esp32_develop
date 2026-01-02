idf_component_register(SRCS "protocol.c" "main.c"
                    PRIV_REQUIRES esp_wifi nvs_flash esp_timer spi_flash
                    INCLUDE_DIRS ".")
