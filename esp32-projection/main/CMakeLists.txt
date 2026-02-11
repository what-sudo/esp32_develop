idf_component_register(SRCS  "main.c"
                        "user_nvs_rw.c"
                        "protocol.c"
                        "bemfa.c"
                        "user_http_client.c"
                        "user_http_server.c"
                        "led.c"
                        "pwm.c"
                        "projection.c"
                    REQUIRES
                        esp_wifi
                        nvs_flash
                        esp_timer
                        spi_flash
                        esp-tls
                        esp_http_client
                        esp_http_server
                        esp_driver_ledc
                        esp_driver_rmt
                        my_rmt_transmit
                    INCLUDE_DIRS "."
                    EMBED_FILES "index.html"
                    )
