idf_component_register(SRCS "alarm_system.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt freertos driver nvs_flash)

