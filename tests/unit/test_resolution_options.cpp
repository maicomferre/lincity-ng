#include "../tiny_test.hpp"

#include <vector>

#include "lincity-ng/ResolutionOptions.hpp"

TTEST(resolution_options_filter_sort_and_deduplicate) {
  std::vector<ResolutionOptions::Size> sizes{
      {1280, 1024}, {640, 480}, {1024, 768}, {800, 600},
      {1024, 768}, {799, 900}, {900, 599}};

  ResolutionOptions::normalize(sizes);

  TCHECK_EQ(sizes.size(), static_cast<size_t>(3));
  TCHECK_EQ(sizes[0].first, 800);
  TCHECK_EQ(sizes[0].second, 600);
  TCHECK_EQ(sizes[1].first, 1024);
  TCHECK_EQ(sizes[1].second, 768);
  TCHECK_EQ(sizes[2].first, 1280);
  TCHECK_EQ(sizes[2].second, 1024);
}

TTEST(resolution_options_accepts_minimum_size) {
  TCHECK(ResolutionOptions::isSupported({800, 600}));
  TCHECK(!ResolutionOptions::isSupported({799, 600}));
  TCHECK(!ResolutionOptions::isSupported({800, 599}));
}
