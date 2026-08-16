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
