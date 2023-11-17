file(GLOB SRC_UI ${CMAKE_SOURCE_DIR} "*.cpp" "*.c")

idf_component_register(SRCS ${SRC_UI}
    INCLUDE_DIRS . 
    REQUIRES driver spi_flash nvs_flash esp_eth esp_http_server
       wifi_provisioning json)
