add_executable(lincity-test-sim
  ../headless_env.cpp
  simtest.cpp
)

target_compile_definitions(lincity-test-sim
  PRIVATE
  LINCITYNG_TEST_DATA_DIR="${TEST_DATA_DIR}"
)

add_dependencies(lincity-test-sim test-appdata)

target_link_libraries(lincity-test-sim
  PRIVATE
  # WHOLE_ARCHIVE: the engine modules reference app-level symbols (Mps)
  # that only get pulled out of the archive when something else needs them.
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,LinCity::ng_lib>"
)

add_test(NAME sim COMMAND lincity-test-sim)

# BUG-10: SDL3_ttf/harfbuzz keep process-lifetime shape-plan caches that
# TTF_Quit does not flush and that ASan reports as a ~74MB leak. They are
# third-party caches, not lincity-ng bugs, so we point LSAN at a suppression
# file. The env is inert in the regular (non-ASan) build.
set_tests_properties(sim PROPERTIES
  ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/lsan_suppressions.txt"
)
