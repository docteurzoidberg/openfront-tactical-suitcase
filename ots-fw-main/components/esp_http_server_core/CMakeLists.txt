idf_component_register(
    SRCS "src/http_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_https_server
)
