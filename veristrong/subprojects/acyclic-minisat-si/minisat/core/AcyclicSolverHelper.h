#ifndef MINISAT_SI_ACYCLICSOLVER_HELPER
#define MINISAT_SI_ACYCLICSOLVER_HELPER

#include <vector>
#include <set>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <utility>
#include <functional>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"

// manage polygraph and ICDGraph

namespace MinisatSI {

constexpr static auto pair_hash_endpoint = [](auto &t) {
  const auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

constexpr static auto triple_hash_endpoint2 = [](auto &t) {
  const auto &[t1, t2, t3] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2) ^ h(t3);
};

class AcyclicSolverHelper {
  Polygraph *polygraph;
  ICDGraph icd_graph;
  std::set<std::pair<int, int>> vars_heap;
  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> ww_keys; // (from, to) -> { keys }
  // std::vector<std::unordered_set<int>> ww_to;
  // std::vector<std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)>> wr_to; // <to, key>
  std::vector<std::unordered_map<int64_t, std::unordered_set<int>>> ww_to, wr_to; // from -> (key -> to)

  std::vector<std::stack<std::tuple<int, int, std::pair<int, int>, bool>>> added_edges_of; // <from, to, reason, is_rw>

  // will only be used if INDUCE_KNOWN_EDGE is defined 
  std::vector<std::vector<std::tuple<int, int, std::pair<int, int>, bool>>> known_induced_edges_of; // <from, to, reason, is_rw>

  std::vector<std::multiset<std::pair<int, int>>> dep_edges_of; // to -> (from, reason)
  std::vector<std::multiset<std::tuple<int, int, int>>> anti_dep_edges_of; // from -> (to, ww_reason, wr_reason)

  void construct_wr_cons_propagated_lits(int var);
  bool add_induced_dep_edge(int var, int from, int to, int dep_reason);
  bool add_induced_anti_dep_edge(int var, int from, int to, int ww_reason, int wr_reason);
  void add_known_induced_dep_edge(int from, int to);
  void add_known_induced_anti_dep_edge(int from, int to);
  void remove_induced_dep_edge(int var, int from, int to, int dep_reason);
  void remove_induced_anti_dep_edge(int var, int from, int to, int ww_reason, int wr_reason);

public:
  std::vector<std::vector<Lit>> conflict_clauses;
  std::vector<std::pair<Lit, std::vector<Lit>>> propagated_lits; // <lit, reason>

  AcyclicSolverHelper(Polygraph *_polygraph);
  void add_var(int var);
  void remove_var(int var);
  bool add_edges_of_var(int var);
  void remove_edges_of_var(int var);
  Var get_var_represents_max_edges();
  Var get_var_represents_min_edges();

  // getter
  Polygraph *get_polygraph();
  const int get_level(int x) const;

}; // class AcyclicSolverHelper

namespace Logger {

auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &wr_to_of_from) -> std::string;

}; // namespace MinisatSI::Logger

}
#endif // MINISAT_SI_ACYCLICSOLVER_HELPER