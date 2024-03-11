set(TARGET_NAME ${PROJECT_NAME}-flags)

include(RoqAutogen)

set(NAMESPACE "roq/proxy/fix/flags")

set(AUTOGEN_SCHEMAS auth.json server.json client.json flags.json)

roq_autodoc(
  OUTPUT
  AUTOGEN_DOC
  NAMESPACE
  ${NAMESPACE}
  OUTPUT_TYPE
  "doc"
  FILE_TYPE
  "rstinc"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(flags-autogen-doc ALL DEPENDS ${AUTOGEN_DOC})

roq_autogen(
  OUTPUT
  AUTOGEN_HEADERS
  NAMESPACE
  ${NAMESPACE}
  OUTPUT_TYPE
  "flags"
  FILE_TYPE
  "hpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

add_custom_target(${TARGET_NAME}-autogen-headers ALL DEPENDS ${AUTOGEN_HEADERS})

roq_autogen(
  OUTPUT
  AUTOGEN_SOURCES
  NAMESPACE
  ${NAMESPACE}
  OUTPUT_TYPE
  "flags"
  FILE_TYPE
  "cpp"
  SOURCES
  ${AUTOGEN_SCHEMAS})

roq_gitignore(
  OUTPUT
  .gitignore
  SOURCES
  ${TARGET_NAME}
  ${AUTOGEN_DOC}
  ${AUTOGEN_HEADERS}
  ${AUTOGEN_SOURCES})

add_library(${TARGET_NAME} OBJECT ${AUTOGEN_SOURCES})

add_dependencies(${TARGET_NAME} ${TARGET_NAME}-autogen-headers)

target_link_libraries(${TARGET_NAME} absl::flags)

# install

if(BUILD_DOCS)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${PROJECT_NAME}/rst
  FILES_MATCHING
  PATTERN "*.rstinc"
  PATTERN "CMakeFiles" EXCLUDE)

endif()
