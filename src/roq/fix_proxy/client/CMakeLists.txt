set(TARGET_NAME ${PROJECT_NAME}-client)

add_subdirectory(fix)
add_subdirectory(json)

set(SOURCES manager.cpp)

add_library(${TARGET_NAME} OBJECT ${SOURCES})

if(APPLE)
  target_compile_definitions(${TARGET_NAME} PRIVATE FMT_USE_NONTYPE_TEMPLATE_ARGS=1)
endif()

target_link_libraries(${TARGET_NAME} roq-api::roq-api fmt::fmt)
