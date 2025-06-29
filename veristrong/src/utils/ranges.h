#ifndef CHECKER_UTILS_RANGES_H
#define CHECKER_UTILS_RANGES_H

#include <iterator>
#include <ranges>

namespace checker::utils {

template <std::input_iterator I>
inline constexpr auto as_range(std::pair<I, I> p) -> decltype(auto) {
  return std::ranges::subrange(p.first, p.second);
}

}  // namespace checker::utils

#endif  // CHECKER_UTILS_RANGES_H
