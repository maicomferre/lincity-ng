/* tiny_test.hpp - minimal dependency-free test framework for lincity-ng tests.
 *
 * Usage:
 *   #include "tiny_test.hpp"
 *   TTEST(my_test) { TCHECK_EQ(1 + 1, 2); }
 *
 * Link tests/tiny_main.cpp into the test executable to provide main().
 *
 * Optional CLI flags (parsed by the test's own main, then forwarded here):
 *   --filter <substr>   run only tests whose name contains <substr>
 *   --ts                prepend a timestamp to the [ RUN  ]/[  OK  ] lines
 *   --list-tests        print the test names and exit
 */

#ifndef LINCITYNG_TESTS_TINY_TEST_HPP
#define LINCITYNG_TESTS_TINY_TEST_HPP

#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace tiny_test {

struct Test {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Test>& registry() {
  static std::vector<Test> tests;
  return tests;
}

inline int& failure_count() {
  static int failures = 0;
  return failures;
}

/* --filter / --ts state; main() forwards the CLI flags here. */
inline std::string& filter() {
  static std::string filter_str;
  return filter_str;
}
inline bool& timestamps() {
  static bool ts = false;
  return ts;
}

inline void set_filter(const std::string& value) { filter() = value; }
inline void set_timestamps(bool value) { timestamps() = value; }

inline std::string now_hhmmss() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf;
  localtime_r(&tt, &tm_buf);
  char ts[16];
  strftime(ts, sizeof(ts), "%H:%M:%S", &tm_buf);
  return ts;
}

inline int list_tests() {
  for(const auto& test : registry())
    std::printf("%s\n", test.name.c_str());
  return 0;
}

inline void report_failure(const char* file, int line,
    const std::string& message) {
  ++failure_count();
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, message.c_str());
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) {
    registry().push_back(Test{name, std::move(fn)});
  }
};

inline int run_all_tests() {
  const std::string filter_str = filter();
  int skipped = 0;
  for(auto& test : registry()) {
    if(!filter_str.empty() && test.name.find(filter_str) == std::string::npos) {
      skipped++;
      continue;
    }
    const std::string stamp = timestamps() ? now_hhmmss() + " " : "";
    std::printf("[ RUN  ] %s%s\n", stamp.c_str(), test.name.c_str());
    int failures_before = failure_count();
    try {
      test.fn();
    } catch(const std::exception& err) {
      std::fprintf(stderr, "  EXCEPTION in %s: %s\n",
        test.name.c_str(), err.what());
      ++failure_count();
    } catch(...) {
      std::fprintf(stderr, "  UNKNOWN EXCEPTION in %s\n", test.name.c_str());
      ++failure_count();
    }
    std::printf(failure_count() == failures_before
      ? "[  OK  ] %s%s\n" : "[ FAIL ] %s%s\n", stamp.c_str(), test.name.c_str());
  }
  if(!filter_str.empty())
    std::printf("filter: %zu/%zu test(s) ran (%d skipped)\n",
      registry().size() - skipped, registry().size(), skipped);
  std::printf("%zu test(s), %d failure(s)\n",
    registry().size() - skipped, failure_count());
  return failure_count() == 0 ? 0 : 1;
}

} // namespace tiny_test

#define TTEST(name) \
  static void tiny_test_fn_##name(); \
  static ::tiny_test::Registrar tiny_test_reg_##name(#name, \
    &tiny_test_fn_##name); \
  static void tiny_test_fn_##name()

#define TCHECK(cond) \
  do { if(!(cond)) \
    ::tiny_test::report_failure(__FILE__, __LINE__, \
      std::string("check failed: ") + #cond); } while(0)

#define TCHECK_EQ(a, b) \
  do { \
    const auto& ta_ = (a); \
    const auto& tb_ = (b); \
    if(!(ta_ == tb_)) { \
      std::ostringstream os_; \
      os_ << "check failed: " #a " == " #b \
          << "  (left: " << ta_ << ", right: " << tb_ << ")"; \
      ::tiny_test::report_failure(__FILE__, __LINE__, os_.str()); \
    } \
  } while(0)

#endif // LINCITYNG_TESTS_TINY_TEST_HPP
