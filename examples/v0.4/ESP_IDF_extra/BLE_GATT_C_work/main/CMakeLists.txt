idf_component_register(SRCS "main.c"
                            "ble_gatt_m/m_ble_low.c"
                            "ble_gatt_m/gatt_spp/gatt_spp.c"
                            "ble_gatt_m/gatt_spp_c/gatt_spp_c.c"

                INCLUDE_DIRS "."
                             "ble_gatt_m"
                             "ble_gatt_m/gatt_spp"
                             "ble_gatt_m/gatt_spp_c"
                             )