idf_component_register(SRCS  "main.c"
                        "user_nvs_rw.c"
                        "protocol.c"
                        "bemfa.c"
                        "user_http_client.c"
                        "user_http_server.c"
                    PRIV_REQUIRES
                        esp_wifi
                        nvs_flash
                        esp_timer
                        spi_flash
                        esp-tls
                        esp_http_client
                        esp_http_server
                    INCLUDE_DIRS "."
                    EMBED_FILES "index.html"
                    )
