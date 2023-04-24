file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "*.cpp")

idf_component_register(SRCS ${SRC_UI}
    INCLUDE_DIRS "."
    REQUIRES json)
