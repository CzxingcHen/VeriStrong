#include <unordered_set>
#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <queue>
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_map>

#include "ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "OptOption.h"
#include "minisat/utils/Monitor.h"

namespace Minisat {

using EdgeInfo = std::pair<int, int>;
using ICDGraphEdge = std::tuple<int, int, int>;

ICDGraph::ICDGraph() {}

void ICDGraph::init(int _n_vertices = 0, int n_vars = 0) {
  // note: currently n_vars is useless
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1);
  in.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
  out.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
  inactive_edges.assign(n, std::unordered_set<EdgeInfo, decltype(edge_info_hash_endpoint)>());
  is_var_unassigned.assign(n_vars, true);
}

void ICDGraph::push_into_var_reachsets(const ReachSet &reach_from, const ReachSet &reach_to) {
  vars_queue.push(int(var_reachsets.size()));
  var_reachsets.push_back(std::make_pair(reach_from, reach_to));
}

void ICDGraph::add_inactive_edge(int from, int to, int label) { inactive_edges[from].insert(std::make_pair(to, label)); }

bool ICDGraph::add_edge(int from, int to, int label, bool need_detect_cycle) { 
  // if a cycle is detected, the edge will not be added into the graph
  if (!need_detect_cycle || !detect_cycle(from, to, label)) {
    out[from].insert(std::make_pair(to, label));
    if (level[from] == level[to]) {
      in[to].insert(std::make_pair(from, label));
    }
    ++m;
    if (m > max_m) max_m = m;
    if (inactive_edges[from].contains(std::make_pair(to, label))) {
      inactive_edges[from].erase(std::make_pair(to, label));
    }

#ifdef CHECK_ACYCLICITY
  if (++check_cnt >= 5000) {
    assert(check_acyclicity());
    check_cnt = 0;
  } 
#endif

    return true;
  }
  return false;
}

void ICDGraph::remove_edge(int from, int to, int label) {
  if (out[from].contains(std::make_pair(to, label))) {
    out[from].erase(std::make_pair(to, label));
  }
  if (in[to].contains(std::make_pair(from, label))) {
    in[to].erase(std::make_pair(from, label));
  }
  --m;
  inactive_edges[from].insert(std::make_pair(to, label));
}

void ICDGraph::get_minimal_cycle(std::vector<Lit> &cur_conflict_clauses) {
  cur_conflict_clauses.clear();
  for (auto lit : conflict_clause) cur_conflict_clauses.push_back(lit);
  conflict_clause.clear();
}

void ICDGraph::get_propagated_lits(std::vector<std::pair<Lit, std::vector<Lit>>> &cur_propagated_lits) {
  cur_propagated_lits.clear();
  for (auto lit : propagated_lits) cur_propagated_lits.push_back(lit);
  propagated_lits.clear();
}

bool ICDGraph::detect_cycle(int from, int to, int label) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }
  if (level[from] < level[to]) return false;

  // step 1. search backward
  std::vector<EdgeInfo> backward_pred(n);
  std::unordered_set<int> backward_visited;
  std::queue<int> q;
  q.push(from), backward_visited.insert(from);
  int delta = (int)std::min(std::pow(n, 2.0 / 3), std::pow(max_m, 1.0 / 2)) / 10 + 1, visited_cnt = 0;
  // int delta = (int)(std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  // TODO: count current n_vertices and n_edges dynamically
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (x == to) return construct_backward_cycle(backward_pred, from, to, label);
    ++visited_cnt;
    if (visited_cnt >= delta) break;
    for (auto &[y, l] : in[x]) {
      if (backward_visited.contains(y)) continue;
      backward_visited.insert(y);
      backward_pred[y] = std::make_pair(x, l);
      q.push(y);
    }
  }
  if (visited_cnt < delta) {
    if (level[from] == level[to]) return false;
    if (level[from] > level[to]) { // must be true
      level[to] = level[from];
    }
  } else { // traverse at least delta arcs
    level[to] = level[from] + 1;
    backward_visited.clear();
    backward_visited.insert(from);
  }
  in[to].clear();

  // step 2. search forward 
  while (!q.empty()) q.pop();
  q.push(to);
  std::vector<EdgeInfo> forward_pred(n);
  std::unordered_set<int> forward_visited;
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (forward_visited.contains(x)) continue;
    for (auto &[y, l] : out[x]) {
      if (backward_visited.contains(y)) {
        forward_pred[y] = std::make_pair(x, l);
        return construct_forward_cycle(backward_pred, forward_pred, from, to, label, y);
      }
      if (level[x] == level[y]) {
        in[y].insert(std::make_pair(x, l));
      } else if (level[y] < level[x]) {
        level[y] = level[x];
        in[y].clear();
        in[y].insert(std::make_pair(x, l));
        forward_pred[y] = std::make_pair(x, l);
        q.push(y);
      }
    }
    forward_visited.insert(x);
  }
#ifdef ENABLE_UEP
  construct_propagated_lits(forward_visited, backward_visited, forward_pred, backward_pred, from, to, label);
#endif  
  return false;
}

void ICDGraph::construct_propagated_lits(std::unordered_set<int> &forward_visited, 
                                         std::unordered_set<int> &backward_visited,
                                         std::vector<EdgeInfo> &forward_pred,
                                         std::vector<EdgeInfo> &backward_pred,
                                         int from, int to, int label) {
#ifdef MONITOR_ENABLED
  Monitor::get_monitor()->construct_uep_count++;
  Monitor::get_monitor()->uep_b_size_sum += backward_visited.size();
  Monitor::get_monitor()->uep_f_size_sum += forward_visited.size();
#endif

  propagated_lits.clear();

#ifdef EXTEND_VERTICES_IN_UEP
  const int MAX_N_EXTEND_VERTICES = 55;
  int count = 0;
  std::queue<int> q;
  for (auto x : forward_visited) q.push(x);
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (++count >= MAX_N_EXTEND_VERTICES) break;
    for (const auto &[y, _] : out[x]) {
      if (forward_visited.contains(y)) continue;
      q.push(y);
      forward_visited.insert(y);
    }
  }
  count = 0;
  while (!q.empty()) q.pop();
  for (auto x : backward_visited) q.push(x);
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (++count >= MAX_N_EXTEND_VERTICES) break;
    for (const auto &[y, _] : in[x]) {
      if (backward_visited.contains(y)) continue;
      q.push(y);
      backward_visited.insert(y);
    }
  }
#endif

#ifdef EXTEND_KG_IN_UEP
  auto intersect_node = [&](std::unordered_set<int> &s1, const ReachSet &s2) -> int {
    for (int x : s1) {
      if (s2.test(x)) return x;
    }
    return -1;
  };
  auto construct_reason_lits = [&](int b_node, int f_node) -> std::vector<Lit> {
    std::vector<int> vars;
    for (int x = b_node; x != from; ) {
      const auto &[pred, l] = backward_pred[x];
      if (l != -1) vars.push_back(l);
      x = pred;
    }
    for (int x = f_node; x != to; ) {
      const auto &[pred, l] = forward_pred[x];
      if (l != -1) vars.push_back(l);
      x = pred;
    }
    vars.push_back(label);
    std::sort(vars.begin(), vars.end());
    vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
    
    std::vector<Lit> reason_lits;
    for (auto v : vars) {
      reason_lits.push_back(~mkLit(v, false));
    }
    return std::move(reason_lits);
  };

  // * random version
//   std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
  const int n_vars = var_reachsets.size();
//   int v_st = rng() % n_vars, v_steps = n_vars / 6;
//   for (int v = v_st; v < v_st + v_steps && v < n_vars; v++) {
//     if (!is_var_unassigned[v]) continue;
//     const auto &[reach_from, reach_to] = var_reachsets[v];
//     if (intersect_with(backward_visited, reach_to) && intersect_with(forward_visited, reach_from)) {
// #ifdef MONITOR_ENABLED
//       Monitor::get_monitor()->propagated_lit_add_times++;
// #endif      
//       propagated_lits.push_back(mkLit((Var)v, false));
//       break;
//     }
//   }

  // * queue version
  
  for (int cnt = 0; cnt < n_vars; cnt++) {
    int v = vars_queue.front(); vars_queue.pop();
    if (!is_var_unassigned[v]) {
      vars_queue.push(v);
      continue;
    }
    const auto &[reach_from, reach_to] = var_reachsets[v];
    int b_node = intersect_node(backward_visited, reach_to);
    if (b_node == -1) {
      vars_queue.push(v);
      continue;
    }
    int f_node = intersect_node(forward_visited, reach_from);
    if (f_node == -1) {
      vars_queue.push(v);
      continue;
    }
    // propagate
    std::vector<Lit> reason_lits = construct_reason_lits(b_node, f_node);
    propagated_lits.push_back(std::make_pair(~mkLit((Var)v, false), reason_lits));

    vars_queue.push(v);
    break;
  }
  return;
#endif

  // ifdef EXTEND_KG_IN_UEP, the rest part of this function would not be executed
  // for (int x : forward_visited) {
  //   for (const auto &[y, label] : inactive_edges[x]) {
  //     if (backward_visited.contains(y)) {
  //       propagated_lits.push_back(mkLit((Var)label, false)); // sign = false: v, sign = true: ~v
  //     }
  //   }
  // }
}

bool ICDGraph::construct_backward_cycle(std::vector<EdgeInfo> &backward_pred, int from, int to, int label) {
  // always return true
  std::vector<int> vars;
  for (int x = to; x != from; ) {
    const auto &[pred, l] = backward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  if (label != -1) vars.push_back(label);
  std::sort(vars.begin(), vars.end());
  vars.erase(unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.push_back(mkLit(v, false));
  }
  return true;
}

bool ICDGraph::construct_forward_cycle(std::vector<EdgeInfo> &backward_pred,
                                       std::vector<EdgeInfo> &forward_pred,
                                       int from, int to, int label, int middle) {
  // always return true
  std::vector<int> vars;
  for (int x = middle; x != to; ) {
    const auto &[pred, l] = forward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  for (int x = middle; x != from; ) {
    const auto &[pred, l] = backward_pred[x];
    if (l != -1) vars.push_back(l);
    x = pred;
  }
  if (label != -1) vars.push_back(label);
  std::sort(vars.begin(), vars.end());
  vars.erase(unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.push_back(mkLit(v, false));
  } 
  return true;
}

bool ICDGraph::check_acyclicity() {
  std::vector<std::vector<int>> g(n, std::vector<int>());
  std::unordered_map<int, std::unordered_map<int, bool>> is_exist;
  for (int from = 0; from < n; from++) {
    for (const auto &[to, _] : out[from]) {
      if (is_exist[from][to]) continue;
      is_exist[from][to] = true;
      g[from].push_back(to);
    }
  }
  std::vector<int> deg(n, 0);
  for (int x = 0; x < n; x++) {
    for (int y : g[x]) {
      ++deg[y];
    }
  }

  std::vector<int> order;
  std::queue<int> q;
  for (int x = 0; x < n; x++) {
    if (!deg[x]) q.push(x);
  }
  while (!q.empty()) {
    int x = q.front(); q.pop();
    order.push_back(x);
    for (int y : g[x]) {
      --deg[y];
      if (!deg[y]) q.push(y);
    }
  }
  return (int(order.size()) == n); 
}

void ICDGraph::set_var_status(int var, bool is_unassgined) { is_var_unassigned[var] = is_unassgined; }

} // namespace Minisat