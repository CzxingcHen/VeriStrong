#ifndef MINISAT_SI_GRAPH_H
#define MINISAT_SI_GRAPH_H

// class Graph will act as a util, mainly for the very first edge elimination and AcyclicTheory to detect cycles

#include <vector>
#include <set>
#include <unordered_map>
#include <queue>
#include <bitset>
#include <algorithm>

namespace MinisatSI {

class Graph {
public:
  int n_vertices; // indexed in [0, n_vertices - 1]
  std::vector<std::vector<int>> edges;
  std::unordered_map<int, std::unordered_map<int, bool>> has_edge;
  
  bool prepared_bridge;
  std::set<std::pair<int, int>> bridges;

  static const int MAX_N_VERTICES = 200000;

  bool prepared_reachability;
  std::vector<std::bitset<MAX_N_VERTICES>> reachability;
  
  Graph(int _n_vertices = 0) {
    n_vertices = _n_vertices;
    edges.assign(n_vertices, std::vector<int>());
    prepared_bridge = false;
    prepared_reachability = false;
  }

  bool add_edge(int from, int to) { 
    if (has_edge[from][to]) return false;
    edges[from].push_back(to); 
    return has_edge[from][to] = true;
  }

  void prepare_bridge() {
    int n = n_vertices;
    std::vector<int> dfn(n, 0), low(n, 0), ins(n, 0), stk(n + 1, 0), bel(n, 0);
    int top = 0, dfsc = 0, n_scc = 0;
    const auto &tarjan = [&](auto &&self, int x) -> void {
      dfn[x] = low[x] = ++dfsc;
      stk[++top] = x, ins[x] = true;
      for (int y : edges[x]) {
        if (!dfn[y]) {
          self(self, y);
          low[x] = std::min(low[x], low[y]);
        } else if (ins[y]) {
          low[x] = std::min(low[x], dfn[y]);
        }
      }
      if (dfn[x] == low[x]) {
        ++n_scc;
        int y;
        do {
          y = stk[top--];
          ins[y] = false;
          bel[y] = n_scc;
        } while (x != y);
      }
    };
    for (int i = 0; i < n; i++) {
      if (!dfn[i]) tarjan(tarjan, i);
    }
    if (!bridges.empty()) bridges.clear();
    for (int x = 0; x < n; x++) {
      for (int y : edges[x]) {
        if (bel[x] != bel[y]) {
          bridges.insert(std::make_pair(x, y));
        } 
      }
    }
    prepared_bridge = true;
  }

  bool is_bridge(int from, int to) {
    if (!prepared_bridge) prepare_bridge();
    return bridges.contains(std::make_pair(from, to));
  }

  void prepare_reachability() {
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
  }

  bool reachable(int from, int to) {
    if (!prepared_reachability) {
      prepare_reachability();
      prepared_reachability = true;
    }
    return reachability.at(from).test(to);
  }

};

} // namespace MinisatSI

#endif // MINISAT_SI_GRAPH_H