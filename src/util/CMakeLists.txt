add_library(xmlutil STATIC
  xmlutil.cpp
  xmlutil.hpp
)
target_link_libraries(xmlutil
  PUBLIC
  LibXml++-5.0::LibXml++-5.0
  PRIVATE
	fmt::fmt
)
target_include_directories(xmlutil INTERFACE ..)

add_library(gettextutil INTERFACE gettextutil.hpp)
target_link_libraries(gettextutil INTERFACE gettext)
target_include_directories(gettextutil INTERFACE ..)

add_library(randutil STATIC randutil.cpp randutil.hpp)
target_include_directories(randutil INTERFACE ..)

add_library(ptrutil INTERFACE ptrutil.hpp)
target_include_directories(ptrutil INTERFACE ..)

add_library(debuglog STATIC debuglog.cpp debuglog.hpp)
# BUG-47: debuglog.cpp usa std::mutex, que resolve para pthread_mutex_*.
# Today it links only because glibc >= 2.34 folded pthreads into libc;
# CMakeLists.txt:41 already ran find_package(Threads) but never linked
# the result anywhere. On an older toolchain this target fails to link.
target_link_libraries(debuglog
  PUBLIC
  fmt::fmt
  Threads::Threads
)
target_include_directories(debuglog INTERFACE ..)

include_directories(.)
