## Tests for lincity-ng (see .devdocs/10-padrao-desenvolvimento.md section 3)
##
## Layer 1 - tests/unit/: pure formula/utility tests
## Layer 2 - tests/sim/ : headless simulation harness (invariants)
## Layer 3 - tests/save/: save round-trip tests

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/lib/cfgpath)
include_directories(${CMAKE_SOURCE_DIR}/tests)

add_subdirectory(unit)
add_subdirectory(sim)
add_subdirectory(save)
