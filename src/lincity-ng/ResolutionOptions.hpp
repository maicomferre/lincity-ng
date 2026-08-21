/* ---------------------------------------------------------------------- *
 * src/lincity-ng/ResolutionOptions.hpp
 * This file is part of Lincity-NG.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * ---------------------------------------------------------------------- */

#ifndef __RESOLUTIONOPTIONS_HPP__
#define __RESOLUTIONOPTIONS_HPP__

#include <algorithm>
#include <utility>
#include <vector>

namespace ResolutionOptions {

using Size = std::pair<int, int>;

constexpr int MIN_WIDTH = 800;
constexpr int MIN_HEIGHT = 600;

inline bool isSupported(const Size& size) {
  return size.first >= MIN_WIDTH && size.second >= MIN_HEIGHT;
}

inline void normalize(std::vector<Size>& sizes) {
  sizes.erase(std::remove_if(sizes.begin(), sizes.end(),
                             [](const Size& size) { return !isSupported(size); }),
              sizes.end());
  std::sort(sizes.begin(), sizes.end());
  sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
}

} // namespace ResolutionOptions

#endif
