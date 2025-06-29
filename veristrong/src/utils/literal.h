#ifndef CHECKER_UTILS_LITERAL_H
#define CHECKER_UTILS_LITERAL_H

#include <cstddef>
#include <cstdint>

constexpr size_t operator"" _uz(unsigned long long n) {
  return static_cast<size_t>(n);
}
constexpr ptrdiff_t operator"" _z(unsigned long long n) {
  return static_cast<ptrdiff_t>(n);
}
constexpr uint64_t operator"" _u64(unsigned long long n) {
  return static_cast<uint64_t>(n);
}

#endif
