/* Layer 1 tests for pure utility functions of the engine. */

#include "../tiny_test.hpp"

#include <string>

#include "lincity/util.hpp"

TTEST(num_to_ansi_small_values_unscaled) {
  // Unscaled values are formatted with "%4.0f": up to 4 digits, blank-padded.
  TCHECK_EQ(std::string("   0"), num_to_ansi(0));
  TCHECK_EQ(std::string("   1"), num_to_ansi(1));
  TCHECK_EQ(std::string("  -1"), num_to_ansi(-1));
  TCHECK_EQ(std::string("9999"), num_to_ansi(9999));
  TCHECK_EQ(std::string("-9999"), num_to_ansi(-9999));
}

TTEST(num_to_ansi_scaling_starts_above_9999) {
  // Values up to 9999 are printed verbatim (see util.cpp); only above that
  // the number is scaled down into the 0-999 range with a unit letter.
  TCHECK_EQ(std::string("1200"), num_to_ansi(1200));
  TCHECK_EQ(std::string("1500"), num_to_ansi(1500));
  TCHECK_EQ(std::string("10.0k"), num_to_ansi(10000));
}

TTEST(num_to_ansi_scaling_units) {
  TCHECK_EQ(std::string("1.0M"), num_to_ansi(1000000));
  TCHECK_EQ(std::string("1.0G"), num_to_ansi(1000000000));
  TCHECK_EQ(std::string("1.0T"), num_to_ansi(1000000000000LL));
}

TTEST(num_to_ansi_rounding_looks_sane) {
  // Exact digits are pinned here so regressions in the number formatting
  // are caught (the engine uses "%3.1f" plus a scale letter).
  TCHECK_EQ(std::string("15.2M"), num_to_ansi(15200000));
}

TTEST(format_thousands_groups_three_digits) {
  TCHECK_EQ(std::string("0"), format_thousands(0, '\''));
  TCHECK_EQ(std::string("5"), format_thousands(5, '\''));
  TCHECK_EQ(std::string("999"), format_thousands(999, '\''));
  TCHECK_EQ(std::string("1'234"), format_thousands(1234, '\''));
  TCHECK_EQ(std::string("12'345"), format_thousands(12345, '\''));
  TCHECK_EQ(std::string("1'234'567"), format_thousands(1234567, '\''));
}

TTEST(format_thousands_handles_negative) {
  TCHECK_EQ(std::string("-1'234'567"), format_thousands(-1234567, '\''));
  TCHECK_EQ(std::string("-999"), format_thousands(-999, '\''));
}

TTEST(format_thousands_arbitrary_separator) {
  TCHECK_EQ(std::string("1.234.567"), format_thousands(1234567, '.'));
  TCHECK_EQ(std::string("1,234,567"), format_thousands(1234567, ','));
}
