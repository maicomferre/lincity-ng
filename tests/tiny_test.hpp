/* tiny_test.hpp - minimal dependency-free test framework for lincity-ng tests.
 *
 * Usage:
 *   #include "tiny_test.hpp"
 *   TTEST(my_test) { TCHECK_EQ(1 + 1, 2); }
 *
 * Link tests/tiny_main.cpp into the test executable to provide main().
 */

#ifndef LINCITYNG_TESTS_TINY_TEST_HPP
#define LINCITYNG_TESTS_TINY_TEST_HPP

#include <cstdio>
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
  for(auto& test : registry()) {
    std::printf("[ RUN  ] %s\n", test.name.c_str());
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
      ? "[  OK  ] %s\n" : "[ FAIL ] %s\n", test.name.c_str());
  }
  std::printf("%zu test(s), %d failure(s)\n",
    registry().size(), failure_count());
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
