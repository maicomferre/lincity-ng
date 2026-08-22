#include "../tiny_test.hpp"

#include <string>
#include <vector>

#include "lincity/StateFingerprint.hpp"

TTEST(state_fingerprint_hash_is_stable_and_domain_separated) {
  const StateFingerprint128 empty = fingerprintBytes("");
  const StateFingerprint128 abc = fingerprintBytes("abc");

  TCHECK_EQ(empty.hex().size(), std::size_t(32));
  TCHECK_EQ(abc.hex().size(), std::size_t(32));
  TCHECK(empty != abc);
  TCHECK_EQ(fingerprintBytes("abc").hex(), abc.hex());
  TCHECK_EQ(empty.hex(), std::string("61d32176a7d5f721c56dab92f6ffc61e"));
  TCHECK_EQ(abc.hex(), std::string("e6c91b2f8cb87efb1d7cb7e9dfa6ce82"));
}

TTEST(state_snapshot_reports_first_different_section_and_byte) {
  StateSnapshot left({
    {"globals", "abc"},
    {"map", "1234"}
  });
  StateSnapshot equal({
    {"globals", "abc"},
    {"map", "1234"}
  });
  StateSnapshot different({
    {"globals", "abc"},
    {"map", "1294"}
  });

  TCHECK(left.fingerprint() == equal.fingerprint());
  TCHECK(!left.firstDifference(equal).has_value());
  TCHECK(left.fingerprint() != different.fingerprint());

  const auto difference = left.firstDifference(different);
  TCHECK(difference.has_value());
  if(difference) {
    TCHECK_EQ(difference->section, std::string("map"));
    TCHECK_EQ(difference->offset, std::size_t(2));
    TCHECK_EQ(difference->left.value(), static_cast<unsigned char>('3'));
    TCHECK_EQ(difference->right.value(), static_cast<unsigned char>('9'));
  }
}
