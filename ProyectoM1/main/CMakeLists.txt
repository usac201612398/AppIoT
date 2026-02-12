idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."  # Solo el main
    REQUIRES sensores mqtt_service listener wifi
)