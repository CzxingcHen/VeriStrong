#ifndef MINISAT_REDUCE_KNOWN_GRAPH_H
#define MINISAT_REDUCE_KNOWN_GRAPH_H

#include <iostream>
#include <vector>
#include <bitset>
#include <queue>
#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace Minisat {

void reduce_known_graph(int n, std::vector<std::pair<int, int>> &known_edges) {
  static const int MAX_N_VERTICES = 200000;

  if (n > MAX_N_VERTICES) {
    std::cerr << "skip Reduce Known Graph, for n_vertices > MAX_N_VERTICES(this is a magic number, currently 200000)\n";
    return;
  }

  auto exist = std::unordered_map<int, std::unordered_map<int, bool>>{};
  auto edges = std::vector<std::vector<int>>(n);

  for (const auto &[from, to] : known_edges) {
    if (exist[from][to]) continue;
    exist[from][to] = true;
    edges[from].emplace_back(to);
  }

  auto reversed_topo_order = [&]() {
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
    assert(int(order.size()) == n);
    std::reverse(order.begin(), order.end());
    return order;
  }();

  auto f0 = std::vector<std::bitset<MAX_N_VERTICES>>(n);
  auto f1 = std::vector<std::bitset<MAX_N_VERTICES>>(n);
  auto f2 = std::vector<std::bitset<MAX_N_VERTICES>>(n);
  for (const auto &x : reversed_topo_order) {
    f0.at(x).set(x);
    for (const auto &y : edges[x]) {
      f0.at(x) |= f0.at(y);
      f1.at(x) |= f0.at(y);
      f2.at(x) |= f1.at(y);
    }
  }

  auto reduced_edges = std::vector<std::pair<int, int>>{};
  exist.clear();
  for (const auto &[from, to] : known_edges) {
    if (exist[from][to]) continue;
    exist[from][to] = true;

    if (f2.at(from).test(to)) continue;
    reduced_edges.emplace_back(from, to);
  }

  swap(reduced_edges, known_edges);
};

} // namespace Minisat

#endif // MINISAT_REDUCE_KNOWN_GRAPH_H