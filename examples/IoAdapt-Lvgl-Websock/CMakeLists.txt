# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-Wno-maybe-uninitialized -Wno-format -DLV_CONF_INCLUDE_SIMPLE=1 -DLV_CONF_PATH=${CMAKE_SOURCE_DIR}/main/lv_conf.h)
project(Esp32IoAdapter-Example-with-LVGL-and-Websocket)

