#include <unordered_set>
#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <queue>
#include <algorithm>
#include <random>
#include <chrono>
#include <fmt/format.h>
#include <unordered_map>

#include "ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "OptOption.h"
#include "minisat/utils/Monitor.h"
#include "minisat/core/Logger.h"


namespace MinisatSI {

ICDGraph::ICDGraph() {}

#ifndef PK_TOPO_ALGORITHM // default ICD

void ICDGraph::init(int _n_vertices = 0, int n_vars = 0, Polygraph *_polygraph = nullptr) {
  // note: currently n_vars is useless
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1), in.assign(n, {}), out.assign(n, {});
  assigned.assign(n_vars, false);
  polygraph = _polygraph;
}

bool ICDGraph::preprocess() { return true; } // for ICD, do nothing

bool ICDGraph::add_known_edge(int from, int to) { // reason default set to (-1, -1)
  // add_known_edge should not be called after initialisation
  if (!reasons_of[from][to].empty()) return true;
  reasons_of[from][to].insert({-1, -1, -1});
  out[from].insert(to);
  if (level[from] == level[to]) in[to].insert(from);
  if (++m > max_m) max_m = m;
  return true; // always returns true
}

bool ICDGraph::add_edge(int from, int to, std::tuple<int, int, int> reason) { 
  // if a cycle is detected, the edge will not be added into the graph
  Logger::log(fmt::format("   - ICDGraph: adding {} -> {}, reason = ({}, {}, {})", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
  if (reasons_of.contains(from) && reasons_of[from].contains(to) && !reasons_of[from][to].empty()) {
    reasons_of[from][to].insert(reason);
    Logger::log(fmt::format("   - existing {} -> {}, adding ({}, {}, {}) into reasons", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
    Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
    return true;
  }
  Logger::log(fmt::format("   - new edge {} -> {}, detecting cycle", from, to));
  if (!detect_cycle(from, to, reason)) {
    Logger::log("   - no cycle, ok to add edge");
    reasons_of[from][to].insert(reason);
    Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));

    // {
    //   // record levels
    //   Logger::log("   - now level: ", "");
    //   for (auto x : level) Logger::log(fmt::format("{}", x), ", ");
    //   Logger::log("");
    // }

    out[from].insert(to);
    if (level[from] == level[to]) in[to].insert(from);
    if (++m > max_m) max_m = m;
    return true;
  }
  Logger::log("   - cycle! edge is not added");
  return false;
}

void ICDGraph::remove_edge(int from, int to, std::tuple<int, int, int> reason) {
  Logger::log(fmt::format("   - ICDGraph: removing {} -> {}, reason = ({}, {}, {})", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
  assert(reasons_of.contains(from));
  assert(reasons_of[from].contains(to));
  auto &reasons = reasons_of[from][to];
  if (!reasons.contains(reason)) {
    Logger::log(fmt::format("   - !assertion failed, now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
    std::cout << std::endl; // force to flush
  }
  assert(reasons.contains(reason));
  reasons.erase(reasons.find(reason));
  Logger::log(fmt::format("   - removing reasons ({}, {}, {}) in reasons_of[{}][{}]", std::get<0>(reason), std::get<1>(reason), std::get<2>(reason), from, to));
  Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
  if (reasons.empty()) {
    Logger::log(fmt::format("   - empty reasons! removing {} -> {}", from, to));
    if (out[from].contains(to)) out[from].erase(to);
    if (in[to].contains(from)) in[to].erase(from);
    --m;
  }
}

bool ICDGraph::detect_cycle(int from, int to, std::tuple<int, int, int> reason) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }

  if (level[from] < level[to]) return false;

  // adding a conflict edge(which introduces a cycle) may violate the pseudo topoorder 'level', we have to revert them.
  auto level_backup = std::vector<int>(n, 0);
  auto level_changed_nodes = std::unordered_set<int>{};
  auto update_level = [&level_backup, &level_changed_nodes](int x, int new_level, std::vector<int> &level) -> bool {
    if (level_changed_nodes.contains(x)) {
      level[x] = new_level;
      return false; // * note: upd_level will return if x is been added into changed_nodes, not success of update
    }
    level_backup[x] = level[x];
    level_changed_nodes.insert(x);
    level[x] = new_level;
    return true;
  };
  auto revert_level = [&level_backup, &level_changed_nodes](std::vector<int> &level) -> bool {
    // always returns true
    for (auto x : level_changed_nodes) {
      level[x] = level_backup[x];
    }
    return true;
  };
  
  // step 1. search backward
  std::vector<int> backward_pred(n);
  std::unordered_set<int> backward_visited;
  std::queue<int> q;
  q.push(from), backward_visited.insert(from);
  int delta = (int)std::min(std::pow(n, 2.0 / 3), std::pow(max_m, 1.0 / 2)) / 10 + 1, visited_cnt = 0;
  // int delta = (int)(std::pow(max_m, 1.0 / 2)) / 8 + 1, visited_cnt = 0;
  // TODO: count current n_vertices and n_edges dynamically
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (x == to) return construct_backward_cycle(backward_pred, from, to, reason);
    ++visited_cnt;
    if (visited_cnt >= delta) break;
    for (const auto &y : in[x]) {
      if (backward_visited.contains(y)) continue;
      backward_visited.insert(y);
      backward_pred[y] = x;
      q.push(y);
    }
  }
  if (visited_cnt < delta) {
    if (level[from] == level[to]) return false;
    if (level[from] > level[to]) { // must be true
      // level[to] = level[from];
      update_level(to, level[from], level);
    }
  } else { // traverse at least delta arcs
    // level[to] = level[from] + 1;
    update_level(to, level[from] + 1, level);
    backward_visited.clear();
    backward_visited.insert(from);
  }
  in[to].clear();

  // step 2. search forward 
  while (!q.empty()) q.pop();
  q.push(to);
  std::vector<int> forward_pred(n);
  std::unordered_set<int> forward_visited;
  while (!q.empty()) {
    int x = q.front(); q.pop();
    if (forward_visited.contains(x)) continue;
    for (const auto &y : out[x]) {
      if (backward_visited.contains(y)) {
        forward_pred[y] = x;
        return revert_level(level) && construct_forward_cycle(backward_pred, forward_pred, from, to, reason, y);
      }
      if (level[x] == level[y]) {
        in[y].insert(x);
      } else if (level[y] < level[x]) {
        // level[y] = level[x];
        update_level(y, level[x], level);
        in[y].clear();
        in[y].insert(x);
        forward_pred[y] = x;
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
                                         std::vector<int> &forward_pred,
                                         std::vector<int> &backward_pred,
                                         int from, int to, std::tuple<int, int, int> reason) {
  // TODO later: construct propagated lits
  // now pass
}

bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::tuple<int, int, int> reason) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };
  auto add_reason_var= [&vars, &add_var](const std::tuple<int, int, int> &reason) -> void {
    const auto &[r1, r2, r3] = reason;
    add_var(r1), add_var(r2), add_var(r3);
  };

  for (int x = to; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    add_reason_var(*reasons_of[x][pred].begin());
    x = pred;
  }

  add_reason_var(reason);

  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
    // std::cerr << v << " ";
  }
  // std::cerr << std::endl;
  return true;  // always returns true
}

bool ICDGraph::construct_forward_cycle(std::vector<int> &backward_pred, 
                                       std::vector<int> &forward_pred, 
                                       int from, int to, std::tuple<int, int, int> reason, int middle) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  auto add_reason_var= [&vars, &add_var](const std::tuple<int, int, int> &reason) -> void {
    const auto &[r1, r2, r3] = reason;
    add_var(r1), add_var(r2), add_var(r3);
  };

  for (int x = middle; x != to; ) {
    const int pred = forward_pred[x];
    assert(reasons_of.contains(pred) && reasons_of[pred].contains(x) && !reasons_of[pred][x].empty());
    add_reason_var(*reasons_of[pred][x].begin());
    x = pred;
  }

  for (int x = middle; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    add_reason_var(*reasons_of[x][pred].begin());
    x = pred;
  }

  add_reason_var(reason);
  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
    // std::cerr << v << " ";
  }
  // std::cerr << std::endl;
  return true; // always returns true
}

#else // use PK toposort algorithm

void ICDGraph::init(int _n_vertices = 0, int n_vars = 0, Polygraph *_polygraph = nullptr) {
  // note: currently n_vars is useless
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1), in.assign(n, {}), out.assign(n, {}), vis.assign(n, false); // for PK toposort algorithm, use level as topological order
  assigned.assign(n_vars, false);
  polygraph = _polygraph;
}

bool ICDGraph::preprocess() {
  // For PK toposort algorithm, toposort on known graph and find initial level(ord)
  std::vector<std::vector<int>> edges;
  edges.assign(n, std::vector<int>{});

  for (int x = 0; x < n; x++) {
    for (const auto &y : out[x]) {
      edges[x].emplace_back(y); // copy in
    }
  }

  {
    std::vector<int> order;
    std::vector<int> deg(n, 0);
    for (int x = 0; x < n; x++) {
      for (int y : edges[x]) {
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
      for (int y : edges[x]) {
        --deg[y];
        if (!deg[y]) q.push(y);
      }
    }
    if (int(order.size()) != n) return false; // toposort failed! cycle detected in known graph!
    for (unsigned i = 0; i < order.size(); i++) level[order[i]] = i; 
  };

  return true;
}

bool ICDGraph::add_known_edge(int from, int to) { // reason default set to (-1, -1)
  // add_known_edge should not be called after initialisation
  if (!reasons_of[from][to].empty()) return true;
  reasons_of[from][to].insert({-1, -1, -1});
  out[from].insert(to), in[to].insert(from);
  if (++m > max_m) max_m = m;
  return true; // always returns true
}

bool ICDGraph::add_edge(int from, int to, std::tuple<int, int, int> reason) { 
   // if a cycle is detected, the edge will not be added into the graph
  Logger::log(fmt::format("   - ICDGraph: adding {} -> {}, reason = ({}, {}, {})", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
  if (reasons_of.contains(from) && reasons_of[from].contains(to) && !reasons_of[from][to].empty()) {
    reasons_of[from][to].insert(reason);
    Logger::log(fmt::format("   - existing {} -> {}, adding ({}, {}, {}) into reasons", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
    Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
    return true;
  }
  Logger::log(fmt::format("   - new edge {} -> {}, detecting cycle", from, to));
  if (!detect_cycle(from, to, reason)) {
    Logger::log("   - no cycle, ok to add edge");
    reasons_of[from][to].insert(reason);
    Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));

    // {
    //   // record levels
    //   Logger::log("   - now level: ", "");
    //   for (auto x : level) Logger::log(fmt::format("{}", x), ", ");
    //   Logger::log("");
    // }

    out[from].insert(to), in[to].insert(from);
    if (++m > max_m) max_m = m;
    return true;
  }
  Logger::log("   - cycle! edge is not added");
  return false;
}

void ICDGraph::remove_edge(int from, int to, std::tuple<int, int, int> reason) {
  Logger::log(fmt::format("   - ICDGraph: removing {} -> {}, reason = ({}, {}, {})", from, to, std::get<0>(reason), std::get<1>(reason), std::get<2>(reason)));
  assert(reasons_of.contains(from));
  assert(reasons_of[from].contains(to));
  auto &reasons = reasons_of[from][to];
  if (!reasons.contains(reason)) {
    Logger::log(fmt::format("   - !assertion failed, now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
    std::cout << std::endl; // force to flush
  }
  assert(reasons.contains(reason));
  reasons.erase(reasons.find(reason));
  Logger::log(fmt::format("   - removing reasons ({}, {}, {}) in reasons_of[{}][{}]", std::get<0>(reason), std::get<1>(reason), std::get<2>(reason), from, to));
  Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
  if (reasons.empty()) {
    Logger::log(fmt::format("   - empty reasons! removing {} -> {}", from, to));
    if (out[from].contains(to)) out[from].erase(to);
    if (in[to].contains(from)) in[to].erase(from);
    --m;
  }
}

bool ICDGraph::detect_cycle(int from, int to, std::tuple<int, int, int> reason) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }

  if (to == from) {
    {
      // construct self loop
      auto vars = std::vector<int>{};
      const auto &[r1, r2, r3] = reason;
      if (r1 != -1) vars.emplace_back(r1);
      if (r2 != -1) vars.emplace_back(r2);
      if (r3 != -1) vars.emplace_back(r3);
      assert(!vars.empty());

      std::sort(vars.begin(), vars.end());
      vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
      for (auto v : vars) {
        conflict_clause.emplace_back(mkLit(v));
      }
    }
    return true; // self loop!
  } 
  // TODO: PK toposort algorithm
  int lower_bound = level[to], upper_bound = level[from];
  if (lower_bound < upper_bound) {
    bool cycle = false;
    auto forward_visit = std::vector<int>{};
    auto pre = std::vector<int>(n, -1);
    dfs_forward(to, upper_bound, forward_visit, pre, cycle);
    if (cycle) {
      construct_dfs_cycle(from, to, pre, reason);
      return true;
    }
    auto backward_visit = std::vector<int>{};
    dfs_backward(from, lower_bound, backward_visit);
    reorder(forward_visit, backward_visit);
  }
  return false;
}

void ICDGraph::construct_dfs_cycle(int from, int to, std::vector<int> &pre, std::tuple<int, int, int> &reason) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  for (int x = from; x != to; ) {
    assert(x != -1);
    int pred = pre[x];
    assert(pred != -1);
    assert(reasons_of.contains(pred) && reasons_of[pred].contains(x) && !reasons_of[pred][x].empty());
    const auto [reason1, reason2, reason3] = *reasons_of[pred][x].begin();
    add_var(reason1), add_var(reason2), add_var(reason3);
    x = pred;
  }

  {
    const auto [reason1, reason2, reason3] = reason;
    add_var(reason1), add_var(reason2), add_var(reason3);
  }

  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
  }
}

void ICDGraph::dfs_forward(int x, int upper_bound, std::vector<int> &forward_visit, std::vector<int> &pre, bool &cycle) {
  if (cycle) return;
  vis[x] = true;
  forward_visit.emplace_back(x);
  for (const auto &y : out[x]) {
    if (level[y] == upper_bound) { // y == to
      pre[y] = x;
      cycle = true;
      return;
    }
    if (!vis[y] && level[y] < upper_bound) {
      pre[y] = x;
      dfs_forward(y, upper_bound, forward_visit, pre, cycle);
      if (cycle) return;
    }
  }
} 

void ICDGraph::dfs_backward(int x, int lower_bound, std::vector<int> &backward_visit) {
  vis[x] = true;
  backward_visit.emplace_back(x);
  for (const auto &y : in[x]) {
    if (!vis[y] && lower_bound < level[y]) dfs_backward(y, lower_bound, backward_visit);
  }
}

void ICDGraph::reorder(std::vector<int> &forward_visit, std::vector<int> &backward_visit) {
  sort(forward_visit.begin(), forward_visit.end(), [&](const int &u, const int &v) { return level[u] < level[v]; });
  sort(backward_visit.begin(), backward_visit.end(), [&](const int &u, const int &v) { return level[u] < level[v]; });

  // 1. construct L, load backward_visit and forward_visit sequentially
  auto left_arr = std::vector<int>{};
  for (unsigned i = 0; i < backward_visit.size(); i++) {
    int x = backward_visit[i];
    backward_visit[i] = level[x];
    vis[x] = false;
    left_arr.emplace_back(x);
  }
  for (unsigned i = 0; i < forward_visit.size(); i++) {
    int x = forward_visit[i];
    forward_visit[i] = level[x];
    vis[x] = false;
    left_arr.emplace_back(x);
  }

  // 2. merge forward_visit and backward_visit into R
  auto right_arr = std::vector<int>{};
  for (unsigned i = 0, j = 0; i < backward_visit.size() || j < forward_visit.size(); ) {
    if (i >= backward_visit.size()) {
      right_arr.emplace_back(forward_visit[j++]);
    } else if (j >= forward_visit.size()) {
      right_arr.emplace_back(backward_visit[i++]);
    } else {
      if (backward_visit[i] < forward_visit[j]) {
        right_arr.emplace_back(backward_visit[i++]);
      } else {
        right_arr.emplace_back(forward_visit[j++]);
      }
    }
  }

  // 3. reorder
  assert(left_arr.size() == right_arr.size());
  for (unsigned i = 0; i < left_arr.size(); i++) {
    level[left_arr[i]] = right_arr[i];
  }
}

void ICDGraph::construct_propagated_lits(std::unordered_set<int> &forward_visited, 
                                         std::unordered_set<int> &backward_visited,
                                         std::vector<int> &forward_pred,
                                         std::vector<int> &backward_pred,
                                         int from, int to, std::tuple<int, int, int> reason) {
  // TODO later: construct propagated lits
  // now pass
}


bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::tuple<int, int, int> reason) {
  // for PK toposort algorithm, This function will never be called
  assert(0);
  return true;  
}

bool ICDGraph::construct_forward_cycle(std::vector<int> &backward_pred, 
                                       std::vector<int> &forward_pred, 
                                       int from, int to, std::tuple<int, int, int> reason, int middle) {
  // for PK toposort algorithm, This function will never be called
  assert(0);
  return true;
}

#endif

void ICDGraph::add_inactive_edge(int from, int to, std::tuple<int, int, int> reason) { 
  // TODO later: now deprecated
}

bool ICDGraph::check_acyclicity() {
  // TODO later: check acyclicity, toposort
  return true;
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

void ICDGraph::set_var_assigned(int var, bool assgined) { assigned[var] = assgined; }

bool ICDGraph::get_var_assigned(int var) { return assigned[var]; }

const int ICDGraph::get_level(int x) const {
  assert(x >= 0 && x < n && x < level.size());
  return level[x];
}

} // namespace MinisatSI

namespace MinisatSI::Logger {

// ! This is a VERY BAD implementation, see Logger.h for more details
auto reasons2str(const std::unordered_multiset<std::tuple<int, int, int>, decltype(triple_hash_endpoint)> &s) -> std::string {
  if (s.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (const auto &[x, y, z] : s) {
    os << "{" << std::to_string(x) << ", " << std::to_string(y) << ", " << std::to_string(z) << "}, ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  return ret; 
}

} // namespace MinisatSI::Logger