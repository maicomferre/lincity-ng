add_executable(lincity-test-unit
  ../tiny_main.cpp
  test_debuglog.cpp
  test_economy.cpp
  test_maptile_frames.cpp
  test_resolution_options.cpp
  test_util.cpp
)

target_link_libraries(lincity-test-unit
  PRIVATE
  # WHOLE_ARCHIVE: engine translation units reference app-level symbols
  # (Mps, getConfig) that only get pulled out of the archive when something
  # else needs them.
  "$<LINK_LIBRARY:WHOLE_ARCHIVE,LinCity::ng_lib>"
  debuglog
)

add_test(NAME unit COMMAND lincity-test-unit)

# The two debug ownership checks in test_maptile_frames.cpp intentionally
# fork and abort in the child process.  LeakSanitizer cannot safely finalize
# an aborting fork child, so keep ASan/UBSan active but disable only its leak
# phase for this test.  The sim/save tests retain leak detection and cover
# the long-lived process paths.
if(CMAKE_CXX_FLAGS MATCHES "fsanitize=address")
  set_tests_properties(unit PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
  )
endif()
