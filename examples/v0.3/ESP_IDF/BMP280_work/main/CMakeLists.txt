idf_component_register(SRCS "main.c"
                            "i2c_thread/m_i2c_thread.c"
                            "bmp280/m_bmp280.c"
                       INCLUDE_DIRS "."
                                    "i2c_thread"
                                    "bmp280")
