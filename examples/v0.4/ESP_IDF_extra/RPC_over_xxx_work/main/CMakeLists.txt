idf_component_register(SRCS "main.c"
                            "wifi_m/m_wifi.c"
                            "wifi_m/m_wifi_low.c"
                            "udp_m/m_udp_rx.c"
                            "udp_m/m_udp_tx.c"

                            "ble_gatt_m/m_ble_low.c"
                            "ble_gatt_m/gatt_spp/gatt_spp.c"

                            "rpc_m/rpc_function.c"
                            "rpc_m/rpc_transport.c"
                            "rpc_m/rpc_data_link.c"
                            "rpc_m/phy/rpc_phy_udp.c"
                            "rpc_m/phy/rpc_phy_spp.c"

                INCLUDE_DIRS "."
                             "wifi_m"
                             "udp_m"
                             "ble_gatt_m"
                             "ble_gatt_m/gatt_spp"
                             "rpc_m"
                             "rpc_m/phy"
                             "rpc_m/include")