set(TARGET_NAME ${PROJECT_NAME})

add_subdirectory(flags)
add_subdirectory(fix)
add_subdirectory(rest)

add_executable(
  ${TARGET_NAME}
  application.cpp
  config.cpp
  controller.cpp
  error.cpp
  settings.cpp
  shared.cpp
  main.cpp)

add_dependencies(${TARGET_NAME} ${TARGET_NAME}-flags-autogen-headers)

target_link_libraries(
  ${TARGET_NAME}
  PRIVATE ${TARGET_NAME}-flags
          ${TARGET_NAME}-fix
          ${TARGET_NAME}-rest
          ${PROJECT_NAME}-third_party-re2
          roq-fix-bridge::roq-fix-bridge-fix
          roq-fix::roq-fix
          roq-web::roq-web
          roq-io::roq-io
          roq-logging::roq-logging
          roq-logging::roq-logging-flags
          roq-flags::roq-flags
          roq-api::roq-api
          tomlplusplus::tomlplusplus
          fmt::fmt
          re2::re2
          ${RT_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE ROQ_PACKAGE_NAME="${PROJECT_NAME}")

if(APPLE)
  target_compile_definitions(${TARGET_NAME} PRIVATE FMT_USE_NONTYPE_TEMPLATE_ARGS=1)
endif()

install(TARGETS ${TARGET_NAME})
