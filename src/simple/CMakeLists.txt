set(TARGET_NAME "${PROJECT_NAME}-simple")

add_subdirectory(flags)

add_executable("${TARGET_NAME}" application.cpp config.cpp controller.cpp session.cpp main.cpp)

target_link_libraries(
  "${TARGET_NAME}"
  PRIVATE "${TARGET_NAME}-flags"
          "${PROJECT_NAME}-tools"
          roq-fix-bridge::roq-fix-bridge-fix
          roq-fix::roq-fix
          roq-client::roq-client
          roq-web::roq-web
          roq-io::roq-io
          roq-logging::roq-logging
          roq-api::roq-api
          fmt::fmt
          absl::flags)

target_compile_definitions("${TARGET_NAME}" PRIVATE ROQ_PACKAGE_NAME="${PROJECT_NAME}")

install(TARGETS ${TARGET_NAME})
