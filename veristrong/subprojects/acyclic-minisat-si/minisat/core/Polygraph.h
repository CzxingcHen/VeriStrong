#ifndef MINISAT_SI_POLYGRAPH_H
#define MINISAT_SI_POLYGRAPH_H

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

namespace MinisatSI {

class Polygraph {
  using WWVarInfo = std::tuple<int, int, std::set<int64_t>>; // <from, to, keys>
  using WRVarInfo = std::tuple<int, int, int64_t>; // <from, to, key>

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

  void set_n_vars(int n) { n_vars = n; }

  bool is_ww_var(int var) { return ww_info.contains(var); }

  bool is_wr_var(int var) { return wr_info.contains(var); }
  
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
};

} // namespace MinisatSI

#endif // MINISAT_SI_POLYGRAPH_H