add_executable(lincity-test-unit
  ../tiny_main.cpp
  test_economy.cpp
  test_util.cpp
)

target_link_libraries(lincity-test-unit
  PRIVATE
  # WHOLE_ARCHIVE: engine translation units reference app-level symbols
  # (Mps, getConfig) that only get pulled out of the archive when something
  # else needs them.
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,LinCity::ng_lib>"
)

add_test(NAME unit COMMAND lincity-test-unit)
