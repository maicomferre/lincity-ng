add_executable(lincity-test-save
  ../headless_env.cpp
  savetest.cpp
)

target_compile_definitions(lincity-test-save
  PRIVATE
  LINCITYNG_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(lincity-test-save
  PRIVATE
  # WHOLE_ARCHIVE: the engine modules reference app-level symbols (Mps)
  # that only get pulled out of the archive when something else needs them.
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,LinCity::ng_lib>"
)

add_test(NAME save COMMAND lincity-test-save)
