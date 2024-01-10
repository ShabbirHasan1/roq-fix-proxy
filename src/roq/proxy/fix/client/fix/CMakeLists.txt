set(TARGET_NAME ${PROJECT_NAME}-client-fix)

set(SOURCES listener.cpp session.cpp)

add_library(${TARGET_NAME} OBJECT ${SOURCES})

add_dependencies(${TARGET_NAME} ${PROJECT_NAME}-flags-autogen-headers)

target_link_libraries(${TARGET_NAME} roq-api::roq-api fmt::fmt)
