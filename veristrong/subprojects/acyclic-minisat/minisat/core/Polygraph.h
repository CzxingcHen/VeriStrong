#ifndef MINISAT_POLYGRAPH_H
#define MINISAT_POLYGRAPH_H

#include <cstdint>
#include <set>
#include <map>
#include <unordered_map>
#include <queue>
#include <vector>
#include <bitset>
#include <utility>
#include <cassert>
#include <iostream>
#include <algorithm>

#include "minisat/utils/Monitor.h"

namespace Minisat {

class Polygraph {
  using WWVarInfo = std::tuple<int, int, std::set<int64_t>>; // <from, to, keys>
  using WRVarInfo = std::tuple<int, int, int64_t>; // <from, to, key>
  using RWVarInfo = std::pair<int, int>; // <from, to>

  // static const int MAX_N_VERTICES = 1000000 + 50;
  static const int MAX_N_VERTICES = 200000 + 50;
  std::vector<std::bitset<MAX_N_VERTICES>> reachability; 

public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  int n_vars; // indexed in [0, n_vars - 1]
  
  std::vector<std::tuple<int, int, int>> known_edges;
  
  std::unordered_map<int, WWVarInfo> ww_info;
  std::unordered_map<int, WRVarInfo> wr_info; // for solver vars

  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> ww_keys; 
  std::unordered_map<int, std::unordered_map<int, std::set<int64_t>>> wr_keys; // for known graph

  std::unordered_map<int, std::unordered_map<int, int>> ww_var_of; // (from, to) -> ww var
  std::unordered_map<int, std::unordered_map<int, std::unordered_map<int64_t, int>>> wr_var_of; // (from, to, key) -> wr var

  std::vector<std::vector<int>> wr_cons;
  std::unordered_map<int, int> wr_cons_index_of; // var -> wr_cons index

  std::unordered_map<int, RWVarInfo> rw_info; // will only be enabled when OUTER_RW_DERIVATION is defined

  std::unordered_map<int, int> txn_distance;
  int n_total_txns, n_sess;
  std::unordered_map<int, int> session_id_of;

  Polygraph(int _n_vertices = 0) { n_vertices = _n_vertices, n_vars = 0; }

  void add_known_edge(int from, int to, int type, const std::vector<int64_t> &keys) { 
    known_edges.push_back({from, to, type});
    if (type == 1) { // WW
      ww_keys[from][to].insert(keys.begin(), keys.end());
      ww_var_of[from][to] = -1; // set known edge implier -1
    } else if (type == 2) { // WR
      wr_keys[from][to].insert(keys.begin(), keys.end());
      for (const auto &key : keys) wr_var_of[from][to][key] = -1;
    }
  }

  void map_wr_var(int var, int from, int to, int64_t key) {
    assert(!wr_info.contains(var));
    wr_info[var] = WRVarInfo{from, to, key};
    wr_var_of[from][to][key] = var;
  }

  void map_ww_var(int var, int from, int to, const std::set<int64_t> &keys) {
    assert(!ww_info.contains(var));
    ww_info[var] = WWVarInfo{from, to, keys};
    ww_var_of[from][to] = var;
  }

  void map_rw_var(int var, int from, int to) {
    assert(!rw_info.contains(var));
    rw_info[var] = RWVarInfo{from, to};
  } 

  void set_n_vars(int n) { n_vars = n; }

  bool is_ww_var(int var) { return ww_info.contains(var); }

  bool is_wr_var(int var) { return wr_info.contains(var); }

  bool is_rw_var(int var) { return rw_info.contains(var); }
  
  bool has_ww_keys(int from, int to) {
    return ww_keys.contains(from) && ww_keys[from].contains(to) && !ww_keys[from][to].empty();
  }

  bool has_wr_keys(int from, int to) {
    return wr_keys.contains(from) && wr_keys[from].contains(to) && !wr_keys[from][to].empty();
  }

  int add_wr_cons(std::vector<int> &cons) {
    int index = wr_cons.size();
    wr_cons.emplace_back(cons);
    return index;
  }

  void map_wr_cons(int v, int index) { wr_cons_index_of[v] = index; }

  std::vector<int> *get_wr_cons(int v) {
    if (!wr_cons_index_of.contains(v)) return nullptr;
    int index = wr_cons_index_of[v];
    return &wr_cons[index];
  }

  bool construct_known_graph_reachablity() {
    if (n_vertices > MAX_N_VERTICES) {
      std::cerr << "Failed to construct known graph reachability for n_vertices(" << n_vertices << " now)"
                << "> magic number MAX_N_VERTICES(200000 now)";
      return false;
    }
    std::vector<std::vector<int>> edges;
    edges.assign(n_vertices, std::vector<int>{});

    for (const auto &[from, to, type] : known_edges) {
      edges[from].emplace_back(to);
    }

    reachability.assign(n_vertices, std::bitset<MAX_N_VERTICES>());

    auto reversed_topo_order = [&]() {
      std::vector<int> order;
      std::vector<int> deg(n_vertices, 0);
      for (int x = 0; x < n_vertices; x++) {
        for (int y : edges[x]) {
          ++deg[y];
        }
      }
      std::queue<int> q;
      for (int x = 0; x < n_vertices; x++) {
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
      assert(int(order.size()) == n_vertices);
      std::reverse(order.begin(), order.end());
      return order;
    }();

    for (int x : reversed_topo_order) {
      reachability.at(x).set(x);
      for (int y : edges[x]) {
        reachability.at(x) |= reachability.at(y);
      }
    }
    return true;
  }   

  bool reachable_in_known_graph(int from, int to) { return reachability.at(from).test(to); }
};

} // namespace Minisat

#endif // MINISAT_POLYGRAPH_H