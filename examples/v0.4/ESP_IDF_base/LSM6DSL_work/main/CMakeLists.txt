idf_component_register(SRCS "main.c"
                            "i2c_thread/m_i2c_thread.c"
                            "lsm6dsl/m_lsm6dsl.c"
                       INCLUDE_DIRS "."
                                    "i2c_thread"
                                    "lsm6dsl")
