idf_component_register(SRCS "main.c"
                            "spi_thread/m_spi_thread.c"
                            "sd_card/m_sd_card.c"
                       INCLUDE_DIRS "."
                                    "spi_thread"
                                    "sd_card")
