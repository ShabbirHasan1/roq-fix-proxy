set(TARGET_NAME "${PROJECT_NAME}-simple")

add_subdirectory(flags)
add_subdirectory(fix)
add_subdirectory(json)

add_executable("${TARGET_NAME}" application.cpp config.cpp controller.cpp settings.cpp main.cpp)

target_link_libraries(
  "${TARGET_NAME}"
  PRIVATE "${PROJECT_NAME}-flags"
          "${PROJECT_NAME}-fix"
          "${PROJECT_NAME}-json"
          roq-fix-bridge::roq-fix-bridge-fix
          roq-fix::roq-fix
          roq-web::roq-web
          roq-io::roq-io
          roq-logging::roq-logging
          roq-api::roq-api
          tomlplusplus::tomlplusplus
          fmt::fmt
          absl::flags
          ${RT_LIBRARIES})

target_compile_definitions("${TARGET_NAME}" PRIVATE ROQ_PACKAGE_NAME="${PROJECT_NAME}")

install(TARGETS ${TARGET_NAME})
