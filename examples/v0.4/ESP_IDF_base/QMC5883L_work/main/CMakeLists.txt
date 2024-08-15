idf_component_register(SRCS "main.c"
                            "i2c_thread/m_i2c_thread.c"
                            "qmc5883l/m_qmc5883l.c"
                       INCLUDE_DIRS "."
                                    "i2c_thread"
                                    "qmc5883l")
