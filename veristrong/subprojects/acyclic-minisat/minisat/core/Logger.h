#ifndef MINISAT_LOGGER_H
#define MINISAT_LOGGER_H

#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <vector>
#include <set>
#include <unordered_set>

#include "minisat/core/OptOption.h"
#include "minisat/core/SolverTypes.h"

namespace Minisat::Logger {

auto log(std::string str, std::string end = "\n") -> bool;

auto type2str(int type) -> std::string;

// auto wr_to2str() 
// note: the definition of this function is in AcyclicHelper.h, for wr_to uses an anonymous namespace in its declaration

// auto reasons2str() -> std::string;
// note: the definition of this function is in ICDGraph.h, for reasons_of uses an anonymous namespace in its declaration
// ! This is a VERY BAD implementation, see related warning(s) for more details


template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
vector2str(const std::vector<T> &vec);

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
set2str(const std::set<T> &s);

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
urdset2str(const std::unordered_set<T> &s);

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
vector2str(const std::vector<T> &vec) {
  if (vec.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (auto i : vec) os << std::to_string(i) << ", ";
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  return ret; 
}

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
set2str(const std::set<T> &s) {
  if (s.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (auto i : s) {
    // std::cout << std::to_string(i) << std::endl;
    os << std::to_string(i) << ", ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  // std::cout << "\"" << ret << "\"" << std::endl;
  return ret; 
}

template<typename T>
typename std::enable_if<std::is_same<T, int>::value || std::is_same<T, int64_t>::value, std::string>::type 
urdset2str(const std::unordered_set<T> &s) {
  if (s.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (auto i : s) {
    // std::cout << std::to_string(i) << std::endl;
    os << std::to_string(i) << ", ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  // std::cout << "\"" << ret << "\"" << std::endl;
  return ret; 
}

// has been moved into AcyclicSolverHelper.h
// auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &s) -> std::string {
//   if (s.empty()) return std::string{""};
//   auto os = std::ostringstream{};
//   for (auto [x, y] : s) {
//     // std::cout << std::to_string(i) << std::endl;
//     os << "{" << std::to_string(x) << ", " << std::to_string(y) << "}, ";
//   } 
//   auto ret = os.str();
//   ret.erase(ret.length() - 2); // delete last ", "
//   // std::cout << "\"" << ret << "\"" << std::endl;
//   return ret; 
// }

auto lits2str(vec<Lit> &lits) -> std::string;


} // namespace Minisat::Logger

#endif // MINISAT_LOGGER_H