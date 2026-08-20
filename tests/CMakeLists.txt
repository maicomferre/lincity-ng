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
# the compiled .mo locale files are also installed by `cmake --install`
# (when ENABLE_NLS is on), so the locales target must be built first —
# otherwise a fresh parallel build races the install against msgfmt and
# fails with "file INSTALL cannot find .../locale/<lang>/LC_MESSAGES/
# lincity-ng.mo". Incremental builds hide this because the .mo already
# exist from a previous build.
if(TARGET locales)
  add_dependencies(test-appdata locales)
endif()
# same race for the doc files (CHANGELOG, COPYING, README, BUILD_OPTIONS,
# ...): `cmake --install` copies them from the build tree where
# doc-files.target writes them. Without this dependency a fresh build
# can try to install them before the copy finishes.
if(TARGET doc-files.target)
  add_dependencies(test-appdata doc-files.target)
endif()
# `cmake --install` also installs the main executable (install(TARGETS
# lincity-ng ...)). Without depending on it, a fresh parallel build can
# run the install before the binary is linked.
if(TARGET lincity-ng)
  add_dependencies(test-appdata lincity-ng)
endif()

add_subdirectory(unit)
add_subdirectory(sim)
add_subdirectory(save)

# TST-07: the development driver must propagate failures and keep concurrent
# runs isolated. It uses a fake ctest binary and does not require app-data.
add_test(
  NAME devlog-driver
  COMMAND bash ${CMAKE_SOURCE_DIR}/tests/devlog_test.sh ${CMAKE_SOURCE_DIR}
)
