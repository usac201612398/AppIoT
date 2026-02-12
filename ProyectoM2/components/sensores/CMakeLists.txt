idf_component_register(
    SRCS "sensores.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos ultrasonic ds18x20 onewire
)
