idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES sensores mqtt_service listener wifi
)