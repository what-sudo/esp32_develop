idf_component_register(SRCS  "main.c"
                        "user_nvs_rw.c"
                        "protocol.c"
                        "bemfa.c"
                    PRIV_REQUIRES esp_wifi nvs_flash esp_timer spi_flash
                    INCLUDE_DIRS ".")
