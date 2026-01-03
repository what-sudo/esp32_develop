idf_component_register(SRCS  "main.c"
                        "user_nvs_rw.c"
                        "protocol.c"
                        "bemfa.c"
                        "user_http_client.c"
                    PRIV_REQUIRES
                        esp_wifi
                        nvs_flash
                        esp_timer
                        spi_flash
                        esp-tls
                        esp_http_client
                    INCLUDE_DIRS ".")
