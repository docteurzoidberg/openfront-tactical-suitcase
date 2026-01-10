idf_component_register(
    SRCS "can_bus_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES can_driver can_discovery
)
