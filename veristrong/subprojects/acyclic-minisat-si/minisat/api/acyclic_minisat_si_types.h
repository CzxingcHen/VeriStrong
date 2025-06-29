#ifndef MINISAT_SI_ACYCLIC_MINISAT_TYPES_H
#define MINISAT_SI_ACYCLIC_MINISAT_TYPES_H

#include <utility>
#include <vector>
#include <cstdint>

namespace MinisatSI {

using Edge = std::tuple<int, int, int, std::vector<int64_t>>; // <type, from, to, keys>
                                                              // type = 0: SO, keys = {}
                                                              //        1: WW,
                                                              //        2: WR,
                                                              //        3: RW, keys = {}
using KnownGraph = std::vector<Edge>;

using WWConstraint = std::tuple<int, int, std::vector<int64_t>>; // <either, or, keys>
using WRConstraint = std::tuple<int, std::vector<int>, int64_t>; // <read, write(s), key> 
using Constraints = std::pair<std::vector<WWConstraint>, std::vector<WRConstraint>>;

} // namespace MinisatSI

#endif // MINISAT_SI_ACYCLIC_MINISAT_TYPES_H