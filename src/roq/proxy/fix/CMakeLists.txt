set(TARGET_NAME ${PROJECT_NAME})

add_subdirectory(flags)
add_subdirectory(server)
add_subdirectory(client)

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
          ${TARGET_NAME}-server
          ${TARGET_NAME}-client-fix
          ${TARGET_NAME}-client-json
          ${TARGET_NAME}-client
          ${PROJECT_NAME}-third_party-re2
          roq-fix-codec::roq-fix-codec
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

if(ROQ_BUILD_TYPE STREQUAL "Release")
  set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)
endif()

target_compile_definitions(
  ${TARGET_NAME}
  PRIVATE ROQ_PACKAGE_NAME="${TARGET_NAME}" ROQ_HOST="${ROQ_HOST}"
          ROQ_BUILD_VERSION="${GIT_REPO_VERSION}" ROQ_GIT_DESCRIBE_HASH="${GIT_DESCRIBE_HASH}"
          ROQ_BUILD_NUMBER="${ROQ_BUILD_NUMBER}" ROQ_BUILD_TYPE="${ROQ_BUILD_TYPE}")

if(APPLE)
  target_compile_definitions(${TARGET_NAME} PRIVATE FMT_USE_NONTYPE_TEMPLATE_ARGS=1)
endif()

install(TARGETS ${TARGET_NAME})
