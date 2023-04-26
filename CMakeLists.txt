string(REPLACE "\\" "/" HTTPD_PATH $ENV{IDF_PATH}/components/esp_http_server/src)
file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "*.cpp")

idf_component_register(SRCS ${SRC_UI}
     INCLUDE_DIRS . ${HTTPD_PATH} ${HTTPD_PATH}"/port/esp32" 
    ${IDF_PATH}/examples/common_components/protocol_examples_common/include
    REQUIRES driver spi_flash nvs_flash esp_eth esp_http_server
       wifi_provisioning json)
