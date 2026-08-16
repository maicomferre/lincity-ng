## Tests for lincity-ng (see .devdocs/10-padrao-desenvolvimento.md section 3)
##
## Layer 1 - tests/unit/: pure formula/utility tests
## Layer 2 - tests/sim/ : headless simulation harness (invariants)
## Layer 3 - tests/save/: save round-trip tests

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/lib/cfgpath)
include_directories(${CMAKE_SOURCE_DIR}/tests)

# The headless harnesses read the game data (the gui XML needs the
# xsltproc transformation, plus scenarios/images/...), so stage an
# app-data directory inside the build tree without touching any system
# path.
set(TEST_DATA_DIR "${CMAKE_BINARY_DIR}/test-install/share/lincity-ng")
add_custom_target(test-appdata ALL
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
    --component Unspecified
    --prefix ${CMAKE_BINARY_DIR}/test-install
  COMMENT "Staging app data for the tests"
  VERBATIM
)
# the gui XML must be transformed before it can be staged
add_dependencies(test-appdata guiXml guiDialogXml data-files.target)

add_subdirectory(unit)
add_subdirectory(sim)
add_subdirectory(save)
