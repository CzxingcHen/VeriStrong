#ifndef CHECKER_SOLVER_ACYCLICMINISATSOLVER_H
#define CHECKER_SOLVER_ACYCLICMINISATSOLVER_H

#include <vector>
#include <utility>
#include <filesystem>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::solver {

struct AcyclicMinisatSolver : AbstractSolver {
  using AMEdge = std::tuple<int, int, int, std::vector<int64_t>>; // <type, from, to, keys>
                                                                  // see specific type info in "acyclic_minisat_types.h"
  using AMKnownGraph = std::vector<AMEdge>;

  using AMWWConstraint = std::tuple<int, int, std::vector<int64_t>>; // <either, or, keys>
  using AMWRConstraint = std::tuple<int, std::vector<int>, int64_t>; // <read, write(s), key> 
  using AMConstraints = std::pair<std::vector<AMWWConstraint>, std::vector<AMWRConstraint>>;

  std::string target_isolation_level;

  fs::path agnf_path;

  int n_vertices;
  AMKnownGraph am_known_graph;
  AMConstraints am_constraints;
  
  int n_sessions, n_total_transactions, n_total_events;
  std::unordered_map<int, std::unordered_map<int64_t, int>> write_steps, read_steps;
  std::unordered_map<int, int> txn_distance;
  std::unordered_map<int, int> session_id_of;

  AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                       const history::Constraints &constraints,
                       const history::HistoryMetaInfo &history_meta_info,
                       const std::string &isolation_level);

  auto solve() -> bool override;

  ~AcyclicMinisatSolver();
};

} // namespace checker::solver

#endif