#ifndef MINISAT_SI_ICDGRAPH_H
#define MINISAT_SI_ICDGRAPH_H

#include <tuple>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>

#include "minisat/mtl/Vec.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Polygraph.h"

namespace MinisatSI {

constexpr static auto pair_hash_endpoint2 = [](const auto &t) {
  const auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

constexpr static auto triple_hash_endpoint = [](const auto &t) {
  const auto &[t1, t2, t3] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2) ^ h(t3);
};

constexpr int MAX_N = 100000;

class ICDGraph {
  /* implement a graph data structure, to support below operations:
    1. add an edge / remove an edge
    2. detect cycles
    3. find the minimal cycle
  */ 
  
  int n, max_m, m; // n_vertices, n_edges
  std::vector<std::unordered_set<int>> in, out; // in[from], out[from] = {(to, label)}
  std::unordered_map<int, std::unordered_map<int, std::unordered_multiset<std::tuple<int, int, int>, decltype(triple_hash_endpoint)>>> reasons_of; // (from, to) -> {(dep_reason, anti_ww_reason, anti_wr_reason)}
  // "multi" is used to handle conflict drived by known edges, for example, 
  // known graph contains edge WR: 1 -> 2, keys = {1, 2}
  // In some pass, variable v: (WW: 1 -> 3, keys = {1, 2}) is decided to be added,
  // then 2 RW edges are derived, but both reasons are (v, -1), 
  // however, (v, -1) is able to be added into the reasons only once
  // when we decided to remove v from traits, (v, -1) will be removed twice, which drives to conflict
  std::vector<int> level;
  std::vector<Lit> conflict_clause;
  std::vector<std::pair<Lit, std::vector<Lit>>> propagated_lits;
  std::vector<bool> vis;

  std::vector<bool> assigned;

  Polygraph *polygraph;

  // --- deprecated ---
  int check_cnt = 0;
  std::queue<int> vars_queue;
  // ------------------

  bool check_acyclicity();
  bool detect_cycle(int from, int to, std::tuple<int, int, int> reason);
  bool construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::tuple<int, int, int> reason);
  bool construct_forward_cycle(std::vector<int> &backward_pred, 
                               std::vector<int> &forward_pred, 
                               int from, int to, std::tuple<int, int, int> reason, int middle);
  void construct_propagated_lits(std::unordered_set<int> &forward_visited, 
                                 std::unordered_set<int> &backward_visited,
                                 std::vector<int> &forward_pred,
                                 std::vector<int> &backward_pred,
                                 int from, int to, std::tuple<int, int, int> reason);

  void construct_dfs_cycle(int from, int to, std::vector<int> &pre, std::tuple<int, int, int> &reason);
  void dfs_forward(int x, int upper_bound, std::vector<int> &forward_visit, std::vector<int> &pre, bool &cycle);
  void dfs_backward(int x, int lower_bound, std::vector<int> &backward_visit);
  void reorder(std::vector<int> &forward_visit, std::vector<int> &backward_visit);


public:
  ICDGraph();
  void init(int _n_vertices, int n_vars, Polygraph *_polygraph);
  void add_inactive_edge(int from, int to, std::tuple<int, int, int> reason);
  bool add_known_edge(int from, int to); // reason default set to (-1, -1)
  bool add_edge(int from, int to, std::tuple<int, int, int> reason); // add (from, to, label)
  void remove_edge(int from, int to, std::tuple<int, int, int> reason); // remove (from, to, label), assume (from, to, label) in the graph
  void get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses); // get_minimal_cycle will copy conflict_clause and clear it
  void get_propagated_lits(std::vector<std::pair<Lit, std::vector<Lit>>> &cur_propagated_lit); // get_propagated_lits will copy propagated_lits and clear it 
  void set_var_assigned(int var, bool is_unassigned); 
  bool get_var_assigned(int var);
  // * note: this is a bad implementation, for ICDGraph's original responsibility prevent itself from seeing these info.
  bool preprocess(); // call after known edges are initialized, return false if detect cycles(conflict)
  const int get_level(int x) const;
};

} // namespace MinisatSI

namespace MinisatSI::Logger {

auto reasons2str(const std::unordered_multiset<std::tuple<int, int, int>, decltype(triple_hash_endpoint)> &s) -> std::string;

} // namespace MinisatSI::Logger

#endif // MINISAT_SI_ICDGRAPH_H