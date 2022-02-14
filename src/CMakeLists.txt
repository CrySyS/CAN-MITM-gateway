FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(COMPONENT_SRCS ${app_sources})

register_component()

target_add_binary_data(${COMPONENT_TARGET} "web/index.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "web/css/main.css" TEXT)

