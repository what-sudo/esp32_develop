idf_component_register(SRCS "my_rmt_transmit.c" "ir_nec_encoder.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        esp_driver_rmt
                        )
