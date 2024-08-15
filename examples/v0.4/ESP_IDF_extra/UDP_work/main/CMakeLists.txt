idf_component_register(SRCS "main.c"
                            "wifi_m/m_wifi.c"
                            "wifi_m/m_wifi_low.c"
                            "udp_m/m_udp.c"
                INCLUDE_DIRS "."
                             "wifi_m"
                             "udp_m")