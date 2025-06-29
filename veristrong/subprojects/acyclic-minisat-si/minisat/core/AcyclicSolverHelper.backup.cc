#include "AcyclicSolverHelper.h"

#include <iostream>
#include <vector>
#include <bitset>
#include <tuple>
#include <cassert>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/OptOption.h"

namespace MinisatSI {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  polygraph = _polygraph;
  icd_graph.init(polygraph->n_vertices, polygraph->edges.size());
  conflict_clauses.clear();
  bool cycle = false;
  for (const auto &[from, to] : polygraph->known_edges) {
    cycle = !icd_graph.add_edge(from, to, /* label = */ -1, /* need_detect_cycle = */ false); 
    if (cycle) {
      throw std::runtime_error("Oops! Cycles were detected in the known graph");
    }
  }

  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->edges.size();
  for (int i = 0; i < n_vars; i++) vars_heap.insert(std::make_pair(polygraph->edges[i].size(), i));

#ifdef EXTEND_KG_IN_UEP
  if (polygraph->n_vertices > MAX_N) {
    std::cerr << "skip extending known graph in unit edge propagation for n = " 
              << polygraph->n_vertices << " > MAX_N" << std::endl;
    return;
  }
  auto reachability = [&](bool reverse = false) -> std::vector<std::bitset<MAX_N>> {
    int n = polygraph->n_vertices;
    std::vector<std::vector<int>> g(n);

    for (const auto &[from, to] : polygraph->known_edges) {
      if (reverse) g[to].push_back(from);
      else g[from].push_back(to);
    }
    
    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n, 0);
      for (int x = 0; x < n; x++) {
        for (int y : g[x]) {
          ++deg[y];
        }
      }
      std::queue<int> q;
      for (int x = 0; x < n; x++) {
        if (!deg[x]) { q.push(x); }
      }
      while (!q.empty()) {
        int x = q.front(); q.pop();
        order.push_back(x);
        for (int y : g[x]) {
          --deg[y];
          if (!deg[y]) q.push(y);
        }
      }
      assert(int(order.size()) == n);
      std::reverse(order.begin(), order.end());
      return std::move(order);
    }();
    
    auto reachability = std::vector<std::bitset<MAX_N>>(n, std::bitset<MAX_N>());
    for (int x : reversed_topo_order) {
      reachability.at(x).set(x);
      for (int y : g[x]) {
        reachability.at(x) |= reachability.at(y);
      }
    }

    return std::move(reachability);
  };
  auto known_graph_reach_to = reachability();
  auto known_graph_reach_from = reachability(true);

  for (int v = 0; v < n_vars; v++) {
    using ReachSet = std::bitset<MAX_N>;
    auto reach_from = ReachSet{}, reach_to = ReachSet{};
    for (const auto &[from, to] : polygraph->edges.at(v)) {
      reach_from |= known_graph_reach_from.at(from);
      reach_to |= known_graph_reach_to.at(to);
    }
    icd_graph.push_into_var_reachsets(reach_from, reach_to);
    
    // std::cerr << reach_from << " " << reach_to << std::endl;
  }

#endif

}

void AcyclicSolverHelper::add_var(int var) {
  int n_edges = polygraph->edges[var].size();
  if (vars_heap.contains(std::make_pair(n_edges, var))) {
    vars_heap.erase(std::make_pair(n_edges, var));
    icd_graph.set_var_status(var, /* is_unassigned = */ false);
  }
} 

void AcyclicSolverHelper::remove_var(int var) {
  int n_edges = polygraph->edges[var].size();
  vars_heap.insert(std::make_pair(n_edges, var));
  icd_graph.set_var_status(var, /* is_unassigned = */ true);
}

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  const auto &edges_of_var = polygraph->edges[var];
  bool cycle = false;
  std::vector<std::tuple<int, int, int>> added_edges;
  for (const auto &[from, to] : edges_of_var) {
    cycle = !icd_graph.add_edge(from, to, /* label = */ var);
    if (cycle) break;
    added_edges.push_back(std::make_tuple(from, to, var));
  }
  if (!cycle) {
    icd_graph.get_propagated_lits(propagated_lits);
    return true;
  } else {
    std::vector<Lit> cur_conflict_clause;
    icd_graph.get_minimal_cycle(cur_conflict_clause);
    conflict_clauses.push_back(cur_conflict_clause);
    for (const auto &[from, to, label] : added_edges) {
      icd_graph.remove_edge(from, to, label);
    }
    return false;
  }
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
  const auto &edges_of_vars = polygraph->edges[var];
  for (const auto &[from, to] : edges_of_vars) {
    icd_graph.remove_edge(from, to, /* label = */ var);
  }
}

Var AcyclicSolverHelper::get_var_represents_max_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = --vars_heap.end();
  return Var(it->second);
}

Var AcyclicSolverHelper::get_var_represents_min_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = vars_heap.begin();
  return Var(it->second);
}

} // namespace MinisatSI