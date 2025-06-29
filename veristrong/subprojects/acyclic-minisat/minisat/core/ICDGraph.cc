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


namespace Minisat {

ICDGraph::ICDGraph() {}

#ifndef PK_TOPO_ALGORITHM // default ICD

void ICDGraph::init(int _n_vertices = 0, int n_vars = 0, Polygraph *_polygraph = nullptr) {
  // note: currently n_vars is useless
  n = _n_vertices, m = max_m = 0;
  level.assign(n, 1), in.assign(n, {}), out.assign(n, {});
  assigned.assign(n_vars, false);
  polygraph = _polygraph;
}

bool ICDGraph::preprocess() { 
  // for ICD, do nothing
  return true; 
}

bool ICDGraph::add_known_edge(int from, int to) { // reason default set to (-1, -1)
  // add_known_edge should not be called after initialisation
  if (!reasons_of[from][to].empty()) return true;
  reasons_of[from][to].insert({-1, -1});
  out[from].insert(to);
  if (level[from] == level[to]) in[to].insert(from);
  if (++m > max_m) max_m = m;
  return true; // always returns true
}

bool ICDGraph::add_edge(int from, int to, std::pair<int, int> reason) { 
  // if a cycle is detected, the edge will not be added into the graph
  Logger::log(fmt::format("   - ICDGraph: adding {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
  if (reasons_of.contains(from) && reasons_of[from].contains(to) && !reasons_of[from][to].empty()) {
    reasons_of[from][to].insert(reason);
    Logger::log(fmt::format("   - existing {} -> {}, adding ({}, {}) into reasons", from, to, reason.first, reason.second));
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

void ICDGraph::remove_edge(int from, int to, std::pair<int, int> reason) {
  Logger::log(fmt::format("   - ICDGraph: removing {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
  assert(reasons_of.contains(from));
  assert(reasons_of[from].contains(to));
  auto &reasons = reasons_of[from][to];
  if (!reasons.contains(reason)) {
    Logger::log(fmt::format("   - !assertion failed, now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
    std::cout << std::endl; // force to flush
  }
  assert(reasons.contains(reason));
  reasons.erase(reasons.find(reason));
  Logger::log(fmt::format("   - removing reasons ({}, {}) in reasons_of[{}][{}]", reason.first, reason.second, from, to));
  Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
  if (reasons.empty()) {
    Logger::log(fmt::format("   - empty reasons! removing {} -> {}", from, to));
    if (out[from].contains(to)) out[from].erase(to);
    if (in[to].contains(from)) in[to].erase(from);
    --m;
  }
}

bool ICDGraph::detect_cycle(int from, int to, std::pair<int, int> reason) {
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
                                         int from, int to, std::pair<int, int> reason) {
  // TODO later: construct propagated lits
  // now pass
}

auto select_reason(const std::unordered_multiset<std::pair<int, int>, decltype(pair_hash_endpoint2)> &reasons) -> std::pair<int, int> {
  int cnt = 3;
  auto reason = std::pair<int, int>{};
  for (const auto &[r1, r2] : reasons) {
    int cur = 0;
    if (r1 != -1) ++cur;
    if (r2 != -1) ++cur;
    if (cur < cnt) {
      reason = {r1, r2};
      cnt = cur;
      if (cnt == 0) return reason;
    } 
  }
  return reason;
}

bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::pair<int, int> reason) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  for (int x = to; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    const auto [reason1, reason2] = *reasons_of[x][pred].begin();
    // const auto [reason1, reason2] = select_reason(reasons_of[x][pred]);
    if (!polygraph->reachable_in_known_graph(x, pred)) {
      add_var(reason1), add_var(reason2);
    }
    x = pred;
  }

  {
    const auto [reason1, reason2] = reason;
    add_var(reason1), add_var(reason2);
  }

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
                                       int from, int to, std::pair<int, int> reason, int middle) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  for (int x = middle; x != to; ) {
    const int pred = forward_pred[x];
    assert(reasons_of.contains(pred) && reasons_of[pred].contains(x) && !reasons_of[pred][x].empty());
    const auto [reason1, reason2] = *reasons_of[pred][x].begin();
    // const auto [reason1, reason2] = select_reason(reasons_of[pred][x]);
    if (!polygraph->reachable_in_known_graph(pred, x)) {
      add_var(reason1), add_var(reason2);
    }
    x = pred;
  }

  for (int x = middle; x != from; ) {
    const int pred = backward_pred[x];
    assert(reasons_of.contains(x) && reasons_of[x].contains(pred) && !reasons_of[x][pred].empty());
    const auto [reason1, reason2] = *reasons_of[x][pred].begin();
    // const auto [reason1, reason2] = select_reason(reasons_of[x][pred]);
    if (!polygraph->reachable_in_known_graph(x, pred)) {
      add_var(reason1), add_var(reason2);
    }
    x = pred;
  }

  {
    const auto [reason1, reason2] = reason;
    add_var(reason1), add_var(reason2);
  }

  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
    // std::cerr << v << " ";
  }
  // std::cerr << std::endl;
  return true; // always returns true
}

// for ICD, below functions do nothing, just left in blank..
void ICDGraph::construct_dfs_cycle(int from, int to, std::vector<int> &pre, std::pair<int, int> &reason) {}
void ICDGraph::dfs_forward(int x, int upper_bound, std::vector<int> &forward_visit, std::vector<int> &pre, bool &cycle) {} 
void ICDGraph::dfs_backward(int x, int lower_bound, std::vector<int> &backward_visit) { }
void ICDGraph::reorder(std::vector<int> &forward_visit, std::vector<int> &backward_visit) {}

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

#ifndef HEURISTIC_DIST_INIT_TOPO
#ifndef HEURISTIC_TOPO_INIT_BY_SESS
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
    
    // for (const auto x : order) {
    //   std::cout << polygraph->session_id_of.at(x) << " ";
    // }
    // std::cout << std::endl;

    for (unsigned i = 0; i < order.size(); i++) level[order[i]] = i; 
  };
#else
#ifndef HEURISTIC_TOPO_INIT_RANDOM
  {
    std::vector<int> order;
    std::vector<int> deg(n, 0);
    for (int x = 0; x < n; x++) {
      for (int y : edges[x]) {
        ++deg[y];
      }
    }

    struct Node { int id, session_id, enq_order; };
    struct NodeCmp {
      bool operator() (const Node &x, const Node&y) const {
        if (x.session_id == y.session_id) return x.enq_order > y.enq_order;
        return x.session_id > y.session_id;
      };
    };
    std::priority_queue<Node, std::vector<Node>, NodeCmp> q;

    int enq_order_cnt = 0;
    for (int x = 0; x < n; x++) {
      if (!deg[x]) { q.push({x, polygraph->session_id_of.at(x), enq_order_cnt++}); }
    }
    while (!q.empty()) {
      auto [x, _1, _2] = q.top(); q.pop();
      order.push_back(x);
      for (int y : edges[x]) {
        --deg[y];
        if (!deg[y]) q.push({y, polygraph->session_id_of.at(y), enq_order_cnt++});
      }
    }
    if (int(order.size()) != n) return false; // toposort failed! cycle detected in known graph!
    // for (const auto x : order) {
    //   std::cout << polygraph->session_id_of.at(x) << " ";
    // }
    // std::cout << std::endl;
    for (unsigned i = 0; i < order.size(); i++) level[order[i]] = i; 
  };
#else // init by random
  {
    std::vector<int> order;
    std::vector<int> deg(n, 0);
    for (int x = 0; x < n; x++) {
      for (int y : edges[x]) {
        ++deg[y];
      }
    }

    struct Node { int id, order; };
    struct NodeCmp {
      bool operator() (const Node &x, const Node&y) const {
        return x.order >= y.order;
      };
    };
    std::priority_queue<Node, std::vector<Node>, NodeCmp> q;

    // Random
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 engine(seed); // Mersenne Twister 随机数引擎
    std::uniform_int_distribution<int> distribution(1, 23333); // 定义范围为 1 到 100
    auto random_number = [&]() -> int { return distribution(engine); };

    for (int x = 0; x < n; x++) {
      if (!deg[x]) { q.push({x, random_number()}); }
    }
    auto rnd_order_of = std::map<int, int>{};
    while (!q.empty()) {
      auto [x, rnd_order] = q.top(); q.pop();
      rnd_order_of[x] = rnd_order;
      order.push_back(x);
      for (int y : edges[x]) {
        --deg[y];
        if (!deg[y]) q.push({y, random_number()});
      }
    }
    if (int(order.size()) != n) return false; // toposort failed! cycle detected in known graph!
    // for (const auto &x : order) {
    //   std::cout << rnd_order_of[x] << " ";
    // }
    // std::cout << std::endl;
    for (unsigned i = 0; i < order.size(); i++) level[order[i]] = i; 
  };
#endif
#endif
#else
  {
    std::vector<int> order;
    std::vector<int> deg(n, 0);
    for (int x = 0; x < n; x++) {
      for (int y : edges[x]) {
        ++deg[y];
      }
    }

    struct Node { int id, dist, enq_order; };
    const int gap = std::max(1, polygraph->n_total_txns / polygraph->n_sess / 20);
    // const int gap = 20;
    struct NodeCmp {
      int gap;
      bool operator() (const Node &x, const Node &y) const {
        if (std::abs(x.dist - y.dist) < gap) {
          return x.enq_order < y.enq_order;
        } else {
          return x.dist < y.dist;
        }
      }
    };
    std::priority_queue<Node, std::vector<Node>, NodeCmp> q(NodeCmp{.gap = gap});
    int enq_order_cnt = 0;
    for (int x = 0; x < n; x++) {
      if (!deg[x]) { q.push({.id = x, .dist = polygraph->txn_distance.at(x), .enq_order = ++enq_order_cnt}); }
    }
    while (!q.empty()) {
      int x = q.top().id; q.pop();
      order.push_back(x);
      for (int y : edges[x]) {
        --deg[y];
        if (!deg[y]) q.push({.id = y, .dist = polygraph->txn_distance.at(y), .enq_order = ++enq_order_cnt});
      }
    }
    if (int(order.size()) != n) return false; // toposort failed! cycle detected in known graph!
    for (unsigned i = 0; i < order.size(); i++) level[order[i]] = i; 
  };
#endif

  return true;
}

bool ICDGraph::add_known_edge(int from, int to) { // reason default set to (-1, -1)
  // add_known_edge should not be called after initialisation
  if (reason_set.any(from, to)) return true;
  const std::pair<int, int> known_reason = {-1, -1};
  reason_set.add_reason_edge(from, to, known_reason);
  out[from].insert(to);
  in[to].insert(from);
  if (++m > max_m) max_m = m;
  return true; // always returns true
}

bool ICDGraph::add_edge(int from, int to, std::pair<int, int> reason) { 
  // if a cycle is detected, the edge will not be added into the graph
  auto start_time = std::chrono::high_resolution_clock::now();

  Logger::log(fmt::format("   - ICDGraph: adding {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
  if (reason_set.any(from, to)) {
    reason_set.add_reason_edge(from, to, reason);
    Logger::log(fmt::format("   - existing {} -> {}, adding ({}, {}) into reasons", from, to, reason.first, reason.second));
    // Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));

    auto end_time = std::chrono::high_resolution_clock::now();
    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->cycle_detection_time += 
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    #endif
    
    return true;
  }
  Logger::log(fmt::format("   - new edge {} -> {}, detecting cycle", from, to));
  if (!detect_cycle(from, to, reason)) {
    Logger::log("   - no cycle, ok to add edge");
    reason_set.add_reason_edge(from, to, reason);
    // Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));

    // {
    //   // record levels
    //   Logger::log("   - now level: ", "");
    //   for (auto x : level) Logger::log(fmt::format("{}", x), ", ");
    //   Logger::log("");
    // }

    out[from].insert(to);
    in[to].insert(from);
    if (++m > max_m) max_m = m;

    auto end_time = std::chrono::high_resolution_clock::now();
    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->cycle_detection_time += 
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    #endif

    return true;
  }
  Logger::log("   - cycle! edge is not added");

  auto end_time = std::chrono::high_resolution_clock::now();
  #ifdef MONITOR_ENABLED
    Monitor::get_monitor()->cycle_detection_time += 
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  #endif

  return false;
}

void ICDGraph::remove_edge(int from, int to, std::pair<int, int> reason) {
  auto start_time = std::chrono::high_resolution_clock::now();

  Logger::log(fmt::format("   - ICDGraph: removing {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
  Logger::log(fmt::format("   - removing reasons ({}, {}) in reasons_of[{}][{}]", reason.first, reason.second, from, to));
  reason_set.remove_reason_edge(from, to, reason);
  // Logger::log(fmt::format("   - now reasons_of[{}][{}] = {}", from, to, Logger::reasons2str(reasons_of[from][to])));
  if (!reason_set.any(from, to)) {
    Logger::log(fmt::format("   - empty reasons! removing {} -> {}", from, to));
    if (out[from].contains(to)) out[from].erase(to);
    if (in[to].contains(from)) in[to].erase(from);
    --m;
  } 

  auto end_time = std::chrono::high_resolution_clock::now();
  #ifdef MONITOR_ENABLED
    Monitor::get_monitor()->cycle_detection_time += 
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  #endif
}

bool ICDGraph::detect_cycle(int from, int to, std::pair<int, int> reason) {
  // if any cycle is detected, construct conflict_clause and return true, else (may skip)construct propagated_lits and return false
  if (!conflict_clause.empty()) {
    std::cerr << "Oops! conflict_clause is not empty when entering detect_cycle()" << std::endl;
    conflict_clause.clear();
  }

  if (to == from) return true; // self loop!

  #ifdef MONITOR_ENABLED
    Monitor::get_monitor()->add_edge_in_icd_graph_times++;
  #endif

  // TODO: PK toposort algorithm
  int lower_bound = level[to], upper_bound = level[from];
  if (lower_bound < upper_bound) {
    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->dfs_when_finding_cycle_in_icd_graph_times++;
    #endif
    
    bool cycle = false;
    auto forward_visit = std::vector<int>{};
    auto pre = std::vector<int>(n, -1);
    dfs_forward(to, upper_bound, forward_visit, pre, cycle);
    if (cycle) {

      #ifdef MONITOR_ENABLED
        Monitor::get_monitor()->find_cycle_in_icd_graph_times++;
      #endif

      construct_dfs_cycle(from, to, pre, reason);

      #ifdef FIND_MINIMAL_CYCLE
        find_minimal_cycle(from, to, reason, pre);
      #endif

      for (const auto &x : forward_visit) vis[x] = false;
      return true;
    }
    auto backward_visit = std::vector<int>{};
    dfs_backward(from, lower_bound, backward_visit);
    reorder(forward_visit, backward_visit);

    #ifdef MONITOR_ENABLED
      swaps_in_pk.emplace_back(std::pair<int, int>{from, to});
    #endif
  }
  return false;
}

void ICDGraph::find_minimal_cycle(int from, int to, std::pair<int, int> &reason, std::vector<int> &pre) {
  {
    auto q = std::priority_queue<std::pair<int, int>>{};
    auto dis = std::vector<int>(n, 0x3fffffff);
    auto extended = std::vector<bool>(n, false);
    q.push({dis[to] = 0, to});
    while (!q.empty()) {
      int x = q.top().second;
      q.pop();
      if (extended[x]) continue;
      extended[x] = true;
      for (const auto &y : out[x]) {
        auto cur_reason = reason_set.get_minimal_reason(x, y);
        int reason_width = 0;
        if (cur_reason.first != -1) ++reason_width;
        if (cur_reason.second != -1) ++reason_width;
        if (dis[y] > dis[x] + reason_width) {
          dis[y] = dis[x] + reason_width;
          q.push({-dis[y], y});
        }
      }
    }
    int width = dis[from];
    if (reason.first != -1) ++width;
    if (reason.second != -1) ++width;
    // note that reason cannot be directly modeled as shortest path, 
    // for var 1 may be added into reason twice

  #ifdef MONITOR_ENABLED
    Monitor::get_monitor()->minimal_cycle_width_count[width]++;
  #endif
  }

  // {
  //   auto path_nodes = std::vector<int>{};
  //   auto dis = std::unordered_map<int, int>{};
  //   const int inf = 0x3f3f3f3f;

  //   for (int x = from; x != to; ) {
  //     assert(x != -1);
  //     int pred = pre[x];
  //     assert(pred != -1);
  //     path_nodes.emplace_back(x);
  //     x = pred;
  //   }
  //   path_nodes.emplace_back(to);

  //   for (const int x : path_nodes) dis[x] = inf;
  //   dis[to] = 0;

  //   std::reverse(path_nodes.begin(), path_nodes.end());

  //   for (size_t i = 0; i < path_nodes.size(); i++) {
  //     int x = path_nodes[i];
  //     for (size_t j = i + 1; j < path_nodes.size(); j++) {
  //       int y = path_nodes[j];
  //       if (reason_set.any(x, y)) {
  //         auto reason = reason_set.get_minimal_reason(x, y);
  //         int reason_width = 0;
  //         if (reason.first != -1) ++reason_width;
  //         if (reason.second != -1) ++reason_width;
  //         dis[y] = std::min(dis[y], dis[x] + reason_width);
  //       }
  //     }
  //   }

  //   int width = dis[from];
  //   if (reason.first != -1) ++width;
  //   if (reason.second != -1) ++width;

  //   #ifdef MONITOR_ENABLED
  //     Monitor::get_monitor()->minimal_cycle_width_count[width]++;
  //   #endif
  // }
}

void ICDGraph::construct_dfs_cycle(int from, int to, std::vector<int> &pre, std::pair<int, int> &reason) {
  auto vars = std::vector<int>{};

  auto add_var = [&vars](int v) -> bool {
    if (v == -1) return false;
    vars.emplace_back(v);
    return true;
  };

  int var_edge_cnt = 0, edge_cnt = 0;
  for (int x = from; x != to; ) {
    assert(x != -1);
    int pred = pre[x];
    assert(pred != -1);

    assert(reason_set.any(pred, x));
    const auto &[reason1, reason2] = reason_set.get_minimal_reason(pred, x);
    if (!polygraph->reachable_in_known_graph(pred, x)) {
      add_var(reason1), add_var(reason2);
      ++edge_cnt;
      if (reason1 != -1 && reason2 != -1) ++var_edge_cnt;
    }
    x = pred;
  }

  {
    const auto [reason1, reason2] = reason;
    add_var(reason1), add_var(reason2);
    ++edge_cnt;
    if (reason1 != -1 && reason2 != -1) ++var_edge_cnt;
  }

#ifdef MONITOR_ENABLED
  Monitor::get_monitor()->var_divide_known_edge_ratio_sum += 1.0 * var_edge_cnt / edge_cnt;
#endif

  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  for (auto v : vars) {
    conflict_clause.emplace_back(mkLit(v));
    // std::cerr << v << " ";
  }
}

void ICDGraph::dfs_forward(int x, int upper_bound, std::vector<int> &forward_visit, std::vector<int> &pre, bool &cycle) {
  if (cycle) return;
  vis[x] = true;
  forward_visit.emplace_back(x);
  for (const auto &y : out[x]) {

    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->dfs_m_times++;
    #endif

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

    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->dfs_m_times++;
    #endif

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
                                         int from, int to, std::pair<int, int> reason) {
  // TODO later: construct propagated lits
  // now pass
}

bool ICDGraph::construct_backward_cycle(std::vector<int> &backward_pred, int from, int to, std::pair<int, int> reason) {
  // for PK toposort algorithm, This function will never be called
  assert(0);
  return true;  
}

bool ICDGraph::construct_forward_cycle(std::vector<int> &backward_pred, 
                                       std::vector<int> &forward_pred, 
                                       int from, int to, std::pair<int, int> reason, int middle) {
  // for PK toposort algorithm, This function will never be called
  assert(0);
  return true;
}

void ICDGraph::calculate_contributed_swaps() {
  // TODO
  int contributed_swap_count = 0;
  for (const auto &[x, y] : swaps_in_pk) {
    // reorder to make level[x] < level[y]
    if (level[x] < level[y]) contributed_swap_count++;
  }
  Monitor::get_monitor()->n_contributed_swaps = contributed_swap_count;
}

#endif

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

void ICDGraph::add_inactive_edge(int from, int to, std::pair<int, int> reason) { 
  // TODO later: now deprecated
}

bool ICDGraph::check_acyclicity() {
  // TODO later: check acyclicity, toposort
  return true;
}

void ICDGraph::set_var_assigned(int var, bool assgined) { assigned[var] = assgined; }

bool ICDGraph::get_var_assigned(int var) { return assigned[var]; }

const int ICDGraph::get_level(int x) const {
  assert(x >= 0 && x < n && x < level.size());
  return level[x];
}

} // namespace Minisat

namespace Minisat::Logger {

// ! This is a VERY BAD implementation, see Logger.h for more details
auto reasons2str(const std::unordered_multiset<std::pair<int, int>, decltype(pair_hash_endpoint2)> &s) -> std::string {
  if (s.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (const auto &[x, y] : s) {
    os << "{" << std::to_string(x) << ", " << std::to_string(y) << "}, ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  return ret; 
}

} // namespace Minisat::Logger