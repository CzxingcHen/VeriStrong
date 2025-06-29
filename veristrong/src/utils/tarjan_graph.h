#ifndef CHECKER_UTILS_TARJAN_GRAPH_H
#define CHECKER_UTILS_TARJAN_GRAPH_H

#include <iostream>
#include <vector>
#include <utility>

namespace checker::utils {

struct TarjanGraph {
  int n, n_scc;
  std::vector<std::vector<int>> g;
  std::vector<int> bel;
  bool prepared;

  TarjanGraph(int _n) {
    n = _n, n_scc = 0, prepared = false;
    g.assign(n, std::vector<int>());
    bel.assign(n, -1);
  }

  auto add_edge(int from, int to) -> void { g[from].emplace_back(to); }

  auto prepare() -> void {
    auto dfn = std::vector<int>(n, 0), low = std::vector<int>(n, 0), stk = std::vector<int>(n + 1, 0);
    auto ins = std::vector<bool>(n, false);
    int top = 0, dfsc = 0;

    const auto &tarjan = [&](auto &&self, int x) -> void {
      dfn[x] = low[x] = ++dfsc;
      stk[++top] = x, ins[x] = true;
      for (int y : g[x]) {
        if (!dfn[y]) {
          self(self, y);
          low[x] = std::min(low[x], low[y]);
        } else if (ins[y]) {
          low[x] = std::min(low[x], dfn[y]);
        }
      }
      if (dfn[x] == low[x]) {
        int y;
        do {
          y = stk[top--];
          ins[y] = false;
          bel[y] = n_scc;
        } while (x != y);
        ++n_scc;
      }
    };

    for (int i = 0; i < n; i++) {
      if (!dfn[i]) tarjan(tarjan, i);
    }

    // for (int i = 0; i < n; i++) std::cerr << bel[i] << " \n"[i + 1 == n];

    prepared = true;
  }

  auto is_bridge(int from, int to) -> bool { 
    if (!prepared) prepare();
    return bel[from] != bel[to];
  }

  auto get_n_scc() -> int { 
    if (!prepared) prepare();
    return n_scc; 
  }

  auto get_scc(int x) -> int {
    if (!prepared) prepare();
    return bel[x];
  }
  
}; // struct TarjanGraph

} // namespace checker::utils

#endif // CHECKER_UTILS_TARJAN_GRAPH_H