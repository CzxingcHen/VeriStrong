#ifndef CHECKER_UTILS_GNF_H
#define CHECKER_UTILS_GNF_H 

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <queue>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <bitset>

#include "utils/literal.h"
#include "history/constraint.h"
#include "history/dependencygraph.h"

#define ENCODE_SMALL_WIDTH_CYCLES

namespace fs = std::filesystem;

namespace checker::utils {
// void write_to_gnf_file(fs::path &gnf_path, 
//                       const history::DependencyGraph &known_graph,
//                       const history::Constraints &constraints) {
  // using GNFEdge = std::pair<int64_t, int64_t>;
  // constexpr auto hash_gnf_edge_endpoint = [](const auto &t) {
  //   auto [t1, t2] = t;
  //   std::hash<int64_t> h;
  //   return h(t1) ^ h(t2);
  // };
  // std::unordered_map<GNFEdge, int64_t, decltype(hash_gnf_edge_endpoint)> edge_id;
  // std::unordered_map<int64_t, GNFEdge> id_edge;
  // std::unordered_map<int64_t, int64_t> node_id;
  // int64_t n_var = 0, n_node = 0;
  // auto alloc_edge_id = [&](const auto &edge) mutable {
  //   if (edge_id.contains(edge)) return;
  //   edge_id[edge] = ++n_var;
  //   id_edge[n_var] = edge;
  // };
  // auto alloc_node_id = [&](const auto &node) mutable {
  //   if (node_id.contains(node)) return;
  //   node_id[node] = n_node++;
  // };
  // auto alloc_edge = [&](const auto &from, const auto &to) {
  //   alloc_edge_id((GNFEdge) {from, to});
  //   alloc_node_id(from), alloc_node_id(to);
  // };
  // int64_t n_clauses = 0;
  // for (const auto &[from, to, _] : known_graph.edges()) {
  //   alloc_edge(from, to);
  //   n_clauses++;
  // } 
  // for (const auto &cons : constraints) {
  //   n_clauses += 4; // (A | B) & (~A | ~B) & (A | ~a1 | ~a2 | ... | ~an) & (B | ~b1 | ~b2 | ... | ~bn) 
  //   for (const auto &[from, to, _] : cons.either_edges) n_clauses++, alloc_edge(from, to); // (~A | a1) & (~A | a2) & ... & (~A | an)
  //   for (const auto &[from, to, _] : cons.or_edges) n_clauses++, alloc_edge(from, to); // (~B | b1) & (~B | b2) & ... & (~B | bn)
  // }
  // int64_t n_edge = n_var, n_edge_set = n_edge;
  // n_var += constraints.size() * 2;

  // std::ofstream ofs;
  // ofs.open(gnf_path, std::ios::out | std::ios::trunc);
  // ofs << "p cnf " << n_var + 1 << " " << n_clauses << std::endl; // n_var + 1 represents the acyclicity
  // for (const auto &[from, to, _] : known_graph.edges()) {
  //   ofs << edge_id[(GNFEdge) {from, to}] << ' ' << 0 << std::endl;
  // }
  
  // for (const auto &cons : constraints) {
  //   int64_t a = ++n_edge_set, b = ++n_edge_set;
  //   ofs << a << " " << b << " " << 0 << std::endl;   // A | B
  //   ofs << -a << " " << -b << " " << 0 << std::endl; // ~A | ~B
  //   ofs << a;
  //   for (const auto &[from, to, _] : cons.either_edges) {
  //     ofs << " " << -edge_id[(GNFEdge) {from, to}];
  //   }
  //   ofs << " " << 0 << std::endl; // A | ~a1 | ~a2 | ... | ~an
  //   ofs << b;
  //   for (const auto &[from, to, _] : cons.or_edges) {
  //     ofs << " " << -edge_id[(GNFEdge) {from, to}];
  //   }
  //   ofs << " " << 0 << std::endl; // B | ~b1 | ~b2 | ... | ~bn
  //   for (const auto &[from, to, _] : cons.either_edges) {
  //     ofs << -a << " " << edge_id[(GNFEdge) {from, to}] << " " << 0 << std::endl;
  //     // (~A | a1) & (~A | a2) & ... & (~A | an)
  //   }
  //   for (const auto &[from, to, _] : cons.or_edges) {
  //     ofs << -b << " " << edge_id[(GNFEdge) {from, to}] << " " << 0 << std::endl;
  //     // (~B | b1) & (~B | b2) & ... & (~B | bn)
  //   }
  // }
  // ofs << n_var + 1 << " " << 0 << std::endl;

  // // describe the graph
  // ofs << "digraph int " << n_node << " " << n_edge << " " << 0 << std::endl;
  // for (int64_t id = 1; id <= n_edge; id++) {
  //   auto &[from, to] = id_edge[id];
  //   ofs << "edge" << " " << 0 << " " << node_id[from] << " " << node_id[to] << " " << id << std::endl;
  // }
  // ofs << "acyclic" << " " << 0 << " " << n_var + 1 << std::endl;

  // ofs.close();
// }

static constexpr auto hash_txns_pair = [](const std::pair<int64_t, int64_t> &p) {
  std::hash<int64_t> h;
  return h(p.first) ^ h(p.second);
};

auto write_to_gnf_file(fs::path &gnf_path, 
                       const history::DependencyGraph &known_graph,
                       const history::Constraints &constraints,
                       const bool enable_unique_encoding = false) -> void {
  const auto &[ww_constraints, wr_constraints] = constraints;

  // 0. generate rw constraints
  struct RWConstraint {
    int64_t from_txn_id, to_txn_id;
    std::unordered_set<std::pair<int64_t, int64_t>, decltype(hash_txns_pair)> reasons; // {<middle, key>}
  };
  auto rw_constraints = [&]() -> std::vector<RWConstraint> {
    auto nodes = std::unordered_set<int64_t>{};
    auto ww_edges = std::unordered_map<int64_t, std::unordered_set<int64_t>>{};
    auto ww_edge_keys = std::unordered_map<std::pair<int64_t, int64_t>, std::unordered_set<int64_t>, decltype(hash_txns_pair)>{};
    auto add_ww_edge = [&](const history::WWConstraint::Edge &edge) -> void {
      const auto &[from, to, edge_info] = edge;
      nodes.insert(from), nodes.insert(to);
      ww_edges[from].insert(to);
      for (const auto &key : edge_info.keys) {
        ww_edge_keys[std::make_pair(from, to)].insert(key);
      }
    };
    for (const auto &[either_txn_id, or_txn_id, either_edges, or_edges] : ww_constraints) {
      add_ww_edge(either_edges.at(0)), add_ww_edge(or_edges.at(0));
    }
    for (const auto &e : known_graph.ww.edges()) {
      add_ww_edge(e);
    }

    auto wr_edges = std::unordered_map<int64_t, std::unordered_set<int64_t>>{};
    auto wr_edge_keys = std::unordered_map<std::pair<int64_t, int64_t>, std::unordered_set<int64_t>, decltype(hash_txns_pair)>{};
    for (const auto &[key, read_txn_id, write_txn_ids] : wr_constraints) {
      nodes.insert(read_txn_id);
      for (const auto &write_txn_id : write_txn_ids) {
        nodes.insert(write_txn_id);
        wr_edges[write_txn_id].insert(read_txn_id);
        wr_edge_keys[std::make_pair(write_txn_id, read_txn_id)].insert(key);
      }
    }
    for (const auto &e : known_graph.wr.edges()) {
      const auto &[from, to, info] = e;
      nodes.insert(from), nodes.insert(to);
      wr_edges[from].insert(to);
      for (const auto &key : info.get().keys) {
        wr_edge_keys[std::make_pair(from, to)].insert(key);
      }
    }

    auto rw_reasons_per_pair = std::unordered_map<std::pair<int64_t, int64_t>, 
                                                  std::unordered_set<std::pair<int64_t, int64_t>, decltype(hash_txns_pair)>, 
                                                  decltype(hash_txns_pair)>{}; // <from, to> -> <middle, key>
    for (const auto &from : nodes) {
      for (const auto &wr_to : wr_edges[from]) {
        for (const auto &ww_to : ww_edges[from]) {
          if (wr_to == ww_to) continue;
          for (const auto &key : wr_edge_keys.at(std::make_pair(from, wr_to))) {
            if (ww_edge_keys[std::make_pair(from, ww_to)].contains(key)) {
              rw_reasons_per_pair[std::make_pair(wr_to, ww_to)].insert(std::make_pair(from, key));
            }
          }
        }
      }
    }

    auto rw_constraints = std::vector<RWConstraint>{};
    for (const auto &[rw_edge, reasons] : rw_reasons_per_pair) {
      const auto &[from, to] = rw_edge;
      rw_constraints.emplace_back(RWConstraint{
                                    .from_txn_id = from, 
                                    .to_txn_id = to,
                                    .reasons = reasons
                                  });
      
      // std::cerr << "from = " << from << ", to = " << to << ", reasons = {";
      // for (const auto &[middle, key] : reasons) {
      //   std::cerr << "(" << middle << ", " << key <<  ") ";
      // }
      // std::cerr << "}\n";
    }
    return rw_constraints;
  }();

  // 1. alloc variable ids
  int n_vars = 0, n_nodes = 0;
  auto id_of_node = std::unordered_map<int64_t, int>{};
  auto edge_of_id = std::unordered_map<int, std::pair<int64_t, int64_t>>{};
  auto id_of_edge = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{};
  auto alloc_node_id = [&](int64_t node) -> void {
    if (id_of_node.contains(node)) return;
    id_of_node[node] = n_nodes++;
  };
  auto alloc_edge_id = [&](int64_t from, int64_t to) -> void {
    alloc_node_id(from), alloc_node_id(to);
    if (id_of_edge.contains(std::make_pair(from, to))) return;
    id_of_edge[std::make_pair(from, to)] = ++n_vars;
    edge_of_id[n_vars] = std::make_pair(from, to);
  };

  // 1.1 e(i, j)
  // 1.1.1 SO edges, and other possible edges in known graph(caused by pruning)
  for (const auto &[from, to, _] : known_graph.edges()) { alloc_edge_id(from, to); } 
  // 1.1.2 WW edges
  for (const auto &[either_txn_id, or_txn_id, _1, _2] : ww_constraints) {
    alloc_edge_id(either_txn_id, or_txn_id),
    alloc_edge_id(or_txn_id, either_txn_id);
  }
  // 1.1.3 WR edges
  for (const auto &[key, read_txn_id, write_txn_ids] : wr_constraints) {
    for (const auto &write_txn_id : write_txn_ids) {
      alloc_edge_id(write_txn_id, read_txn_id);
    }
  }
  // 1.1.4 RW edges
  for (const auto &[from, to, _] : rw_constraints) { alloc_edge_id(from, to); }

  // 1.2 SO, WW, RW(i, j), WR(k, i, j)
  auto so_edge_id = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{};
  auto ww_edge_id = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{};
  auto rw_edge_id = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{};
  auto wr_key_edge_id = std::unordered_map<std::pair<int64_t, int64_t>, 
                                           std::unordered_map<int64_t, int>, 
                                           decltype(hash_txns_pair)>{};
  auto alloc_type_edge_id = [&n_vars](std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)> &type_edge_map, 
                                      int64_t from, int64_t to) -> void {
    if (type_edge_map.contains(std::make_pair(from, to))) return;
    type_edge_map[std::make_pair(from, to)] = ++n_vars;
  };
  for (const auto &[edge, _] : id_of_edge) {
    const auto &[from, to] = edge;
    alloc_type_edge_id(so_edge_id, from, to);
    alloc_type_edge_id(ww_edge_id, from, to);
    alloc_type_edge_id(rw_edge_id, from, to);
  }
  auto alloc_wr_key_edge_id = [&n_vars, &wr_key_edge_id](int64_t from, int64_t to, int64_t key) -> void {
    if (wr_key_edge_id[std::make_pair(from, to)].contains(key)) return;
    wr_key_edge_id[std::make_pair(from, to)][key] = ++n_vars;
  };
  for (const auto &[key, read_txn_id, write_txn_ids] : wr_constraints) {
    for (const auto &write_txn_id : write_txn_ids) {
      alloc_wr_key_edge_id(write_txn_id, read_txn_id, key);
    }
  }
  for (const auto &e : known_graph.wr.edges()) {
    const auto &[from, to, info] = e;
    for (const auto &key : info.get().keys) {
      alloc_wr_key_edge_id(from, to, key);
    }
  }

  // 1.3 edge sets of WW and RW edges
  auto either_edge_set_of_ww_constraint = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{}; 
  auto or_edge_set_of_ww_constraint = std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)>{}; 
  auto alloc_edge_set_id = [&n_vars](std::unordered_map<std::pair<int64_t, int64_t>, int, decltype(hash_txns_pair)> &edge_set_map,
                                     int64_t from, int64_t to) -> void {
    if (edge_set_map.contains(std::make_pair(from, to))) return;
    edge_set_map[std::make_pair(from, to)] = ++n_vars;
  };
  for (const auto &[either_txn_id, or_txn_id, _1, _2] : ww_constraints) {
    alloc_edge_set_id(either_edge_set_of_ww_constraint, either_txn_id, or_txn_id);
    alloc_edge_set_id(or_edge_set_of_ww_constraint, or_txn_id, either_txn_id);
  }
  // 1.4 acyclicity itself
  const auto acyclicity_var = ++n_vars;

  // 2. construct logical clauses
  auto clauses = std::vector<std::vector<int>>{};
  auto add_clause = [&clauses](std::vector<int> clause) { clauses.emplace_back(clause); };
  // 2.1 (SO, WW, RW(i, j), WR(k, i, j)) <=> e(i, j)
  for (const auto &[edge, edge_id] : id_of_edge) {
    add_clause({edge_id, -so_edge_id[edge]});
    add_clause({edge_id, -ww_edge_id[edge]});
    add_clause({edge_id, -rw_edge_id[edge]});
    auto last_clause = std::vector<int>{-edge_id, so_edge_id[edge], ww_edge_id[edge], rw_edge_id[edge]};
    if (wr_key_edge_id.contains(edge)) {
      for (const auto &[key, wr_edge_id] : wr_key_edge_id.at(edge)) {
        last_clause.emplace_back(wr_edge_id);
        add_clause({edge_id, -wr_edge_id});
      }
    }
    add_clause(last_clause);
  }
  // 2.2 edge set of WW and RW edges
  for (const auto &[either_txn_id, or_txn_id, either_edges, or_edges] : ww_constraints) {
    const auto either_var = either_edge_set_of_ww_constraint[std::make_pair(either_txn_id, or_txn_id)];
    const auto or_var = or_edge_set_of_ww_constraint[std::make_pair(or_txn_id, either_txn_id)];
    add_clause({either_var, or_var}), add_clause({-either_var, -or_var});

    auto bind_edge_set = [&](int v, const std::vector<history::WWConstraint::Edge> &edges) -> void {
      auto last_clause = std::vector<int>{v};
      for (auto i = 0_uz; i < edges.size(); i++) {
        const auto &[from, to, edge_info] = edges.at(i);
        int edge_id = 0;
        if (i == 0) { // WW
          assert(ww_edge_id.contains(std::make_pair(from, to)));
          edge_id = ww_edge_id[std::make_pair(from, to)];
          last_clause.emplace_back(-edge_id);
          add_clause({-v, edge_id});
        } else { // WR
          assert(wr_key_edge_id.contains(std::make_pair(from, to)));
          for (const auto &key : edge_info.keys) {
            assert(wr_key_edge_id.at(std::make_pair(from, to)).contains(key));
            edge_id = wr_key_edge_id[std::make_pair(from, to)][key];
            last_clause.emplace_back(-edge_id);
            add_clause({-v, edge_id});
          }
        }
      }

      // for (const auto &[from, to, _] : edges) {
      //   assert(id_of_edge.contains(std::make_pair(from, to)));
      //   int edge_id = id_of_edge[std::make_pair(from, to)];
      //   last_clause.emplace_back(-edge_id);
      //   add_clause({-v, edge_id});
      // }

      add_clause(last_clause);
    }; // bind variable v with edge set, add the logical formula into clauses 
    bind_edge_set(either_var, either_edges), bind_edge_set(or_var, or_edges);
  }
  // 2.3 WR + WW -> RW
  for (const auto &[from, to, reasons] : rw_constraints) {
    for (const auto &[middle, key] : reasons) {
      add_clause({-wr_key_edge_id[std::make_pair(middle, from)][key],
                  -ww_edge_id[std::make_pair(middle, to)],
                  rw_edge_id[std::make_pair(from, to)]});
    }
  }
  // 2.4 WR some
  for (const auto &[key, read_txn_id, write_txn_ids] : wr_constraints) {
    auto clause = std::vector<int>{};
    for (const auto write_txn_id : write_txn_ids) {
      assert(wr_key_edge_id.contains(std::make_pair(write_txn_id, read_txn_id)));
      assert(wr_key_edge_id.at(std::make_pair(write_txn_id, read_txn_id)).contains(key));
      const auto edge_id = wr_key_edge_id[std::make_pair(write_txn_id, read_txn_id)][key];
      clause.emplace_back(edge_id);
    }
    add_clause(clause);
    if (enable_unique_encoding) {
      unsigned n = clause.size();
      for (unsigned i = 0; i < n; i++) {
        for (unsigned j = i + 1; j < n; j++) {
          add_clause({-clause[i], -clause[j]});
        }
      }
    }
  }
  // 2.5 acyclicity
  add_clause({acyclicity_var});
  // 2.6 known edge(SO edge)s
  for (const auto &[from, to, _] : known_graph.edges()) {
    assert(so_edge_id.contains(std::make_pair(from, to)));
    const auto edge_id = so_edge_id[std::make_pair(from, to)];
    add_clause({edge_id});  
  }

  #ifdef ENCODE_SMALL_WIDTH_CYCLES
  const int MAX_N = 100000 + 10;
  if (n_nodes < MAX_N) {
    auto known_edge = std::map<int, bool>{};
    for (const auto &[from, to, _] : known_graph.edges()) {
      assert(id_of_edge.contains(std::make_pair(from, to)));
      const auto edge_id = id_of_edge[std::make_pair(from, to)];
      known_edge[edge_id] = true;
    }    
    auto edge_vars = std::vector<int>{};
    for (const auto &[v, edge] : edge_of_id) {
      if (known_edge[v]) continue;
      edge_vars.emplace_back(v); 
    }

    std::cerr << "#edge vars: " << edge_vars.size() << std::endl;

    auto reachability = [&]() {
      auto edges = std::vector<std::vector<int>>(n_nodes, std::vector<int>{});
      auto deg = std::vector<int>(n_nodes, 0);
      for (const auto &[from, to, _] : known_graph.edges()) {
        edges[id_of_node[from]].emplace_back(id_of_node[to]);
        ++deg[id_of_node[to]];
      }

      auto reversed_topo_order = std::vector<int>{};
      {
        auto q = std::queue<int>{};
        for (int x = 0; x < n_nodes; x++) {
          if (!deg[x]) { q.push(x); }
        }
        while (!q.empty()) {
          int x = q.front();
          reversed_topo_order.emplace_back(x);
          q.pop();
          for (const auto y : edges[x]) {
            --deg[y];
            if (deg[y] == 0) { q.push(y); }
          }
        }
        assert(int(reversed_topo_order.size()) == n_nodes);
        std::reverse(reversed_topo_order.begin(), reversed_topo_order.end());
      }

      auto r = std::vector<std::bitset<MAX_N>>(n_nodes);

      for (const auto &x : reversed_topo_order) {
        r.at(x).set(x);

        for (const auto &y : edges[x]) {
          r.at(x) |= r.at(y);
        }
      }

      return r;
    }();

    auto can_reach = [&reachability, &id_of_node](int64_t from, int64_t to) -> bool {
      return reachability.at(id_of_node[from]).test(id_of_node[to]);
    };

    // 1. self conflict(derived RW edges)
    auto self_conflict = [&](int v) -> bool {
      const auto &[from, to] = edge_of_id[v];
      return can_reach(to, from);
    };

    for (const auto v : edge_vars) {
      if (self_conflict(v)) {
        add_clause({-v});
      }
    }

    auto conflict = [&](int v1, int v2) -> bool {
      const auto &[from1, to1] = edge_of_id[v1];
      const auto &[from2, to2] = edge_of_id[v2];
      return can_reach(to1, from2) && can_reach(to2, from1);
    };

    // 2. conflict(for 2 edges)
    for (const auto v1 : edge_vars) {
      for (const auto v2 : edge_vars) {
        if (v1 >= v2) continue; // assume v1 < v2
        if (conflict(v1, v2)) {
          add_clause({-v1, -v2});
        }
      }
    }
  }
  #endif

  // 3. output to .gnf file
  auto write_file_st_time = std::chrono::steady_clock::now();
  std::ofstream ofs;
  ofs.open(gnf_path, std::ios::out | std::ios::trunc); 
  // 3.1 basic info, like .cnf
  ofs << "p cnf " << n_vars << " " << clauses.size() << "\n";
  for (const auto &clause : clauses) {
    for (const auto &v : clause) { ofs << v << " "; }
    ofs << "0\n";
  }
  // 3.2 graph info
  ofs << "digraph int " << known_graph.num_vertices() << " " << edge_of_id.size() << " 0\n";
  for (const auto &[edge, id] : id_of_edge) {
    const auto &[from, to] = edge;
    assert(id_of_node.contains(from) && id_of_node.contains(to));
    ofs << "edge 0 " << id_of_node.at(from) << " " << id_of_node.at(to) << " " << id << "\n";
  }
  ofs << "acyclic 0 " << acyclicity_var << "\n";
  ofs.close();
  
  auto write_file_ed_time = std::chrono::steady_clock::now();
  // BOOST_LOG_TRIVIAL(info) << "write .gnf time: " << std::chrono::duration_cast<std::chrono::milliseconds>(write_file_ed_time -write_file_st_time);
}
} // namespace checker::utils

#endif // CHECKER_UTILS_GNF_H