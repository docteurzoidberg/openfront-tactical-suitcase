idf_component_register(
    SRCS "can_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)

# Enable physical TWAI hardware support
target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_CAN_DRIVER_USE_TWAI=1)
