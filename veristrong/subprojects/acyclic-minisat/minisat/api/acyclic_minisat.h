#ifndef MINISAT_ACYCLIC_MINISAT_H
#define MINISAT_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>
#include <unordered_map>

#include "minisat/api/acyclic_minisat_types.h"

namespace Minisat {

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, 
              const KnownGraph &known_graph, 
              const Constraints &constraints,
              // history meta info
              const int n_sessions,
              const int n_total_transactions,
              const int n_total_events,
              const std::unordered_map<int, std::unordered_map<int64_t, int>> &write_steps,
              const std::unordered_map<int, std::unordered_map<int64_t, int>> &read_steps,
              const std::unordered_map<int, int> &txn_distance,
              const std::unordered_map<int, int> &session_id_of);

bool am_solve_with_suggestion(int n_vertices, 
                              const KnownGraph &known_graph, 
                              const Constraints &constraints,
                              int suggest_distance,
                              // history meta info
                              const std::unordered_map<int, std::unordered_map<int64_t, int>> &write_steps,
                              const std::unordered_map<int, std::unordered_map<int64_t, int>> &read_steps,
                              const std::unordered_map<int, int> &txn_distance,
                              const int n_sessions, const int n_total_transactions,
                              const std::unordered_map<int, int> &session_id_of);

} // namespace Minisat

#endif // MINISAT_ACYCLIC_MINISAT_H