#ifndef CHECKER_UTILS_TO_VECTOR_H
#define CHECKER_UTILS_TO_VECTOR_H

#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define TO_CONTAINER_DEFINE()

namespace checker::utils {

namespace impl {

template <typename C, typename E>
requires requires(C &c, E &&e) { c.emplace(std::forward<E>(e)); }
auto add(C &c, E &&e) { c.emplace(std::forward<E>(e)); }

template <typename C, typename E>
requires requires(C &c, E &&e) { c.push_back(std::forward<E>(e)); }
auto add(C &c, E &&e) {
  if constexpr (requires(C & c, E && e) {
                  c.emplace_back(std::forward<E>(e));
                }) {
    c.emplace_back(std::forward<E>(e));
  } else {
    c.push_back(std::forward<E>(e));
  }
}

}  // namespace impl

template <typename ConstructorClosure>
struct ToContainerWithConstructor {
  ConstructorClosure c;
  using Container = decltype(c());
};

template <typename Container>
struct ToContainer {
  template <typename... Args>
  constexpr auto operator()(Args &&...args) const {
    return ToContainerWithConstructor{
        .c = [&]() { return Container(std::forward<Args>(args)...); },
    };
  }

  template <std::ranges::range R>
  friend constexpr auto operator|(R &&r, ToContainer t) -> Container {
    return r | t();
  }
};

template <std::ranges::range R, typename T>
constexpr auto operator|(R &&r, ToContainerWithConstructor<T> t) {
  auto c = t.c();

  if constexpr (requires(decltype(c) &c, R && r) {
                  c.reserve(std::ranges::size(r));
                }) {
    c.reserve(std::ranges::size(r));
  }

  std::ranges::for_each(
      r, [&]<typename E>(E &&e) { impl::add(c, std::forward<E>(e)); });

  return c;
}

template <typename C>
inline constexpr auto to = ToContainer<C>{};

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TO_VECTOR_H
