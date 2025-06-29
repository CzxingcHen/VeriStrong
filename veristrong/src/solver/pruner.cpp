#include "pruner.h"

#include <boost/dynamic_bitset.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <iostream>
#include <chrono>
#include <malloc.h>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/ranges.h"
#include "utils/to_container.h"

using boost::add_edge;
using boost::adjacency_list;
using boost::adjacency_matrix;
using boost::dynamic_bitset;
using boost::no_named_parameters;
using boost::not_a_dag;
using boost::out_edges;
using boost::target;
using boost::topological_sort;
using checker::history::WWConstraint;
using checker::history::WRConstraint;
using checker::history::Constraints;
using checker::history::DependencyGraph;
using checker::history::EdgeType;
using checker::history::EdgeInfo;
using checker::utils::as_range;
using checker::utils::to;
using std::back_inserter;
using std::nullopt;
using std::optional;
using std::unordered_set;
using std::vector;
using std::set;
using std::pair;
using std::unordered_map;
using std::ranges::copy;
using std::ranges::views::filter;

namespace chrono = std::chrono;

namespace checker::solver {

auto prune_unit_constraints(DependencyGraph &dependency_graph,
                            Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });

  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }
  auto &&i_vertex_map = unordered_map<int, int64_t>{}; // inversed vertex map
  for (auto &&[v, desc] : vertex_map) i_vertex_map[desc] = v; 
  
  auto rw_edges = unordered_map<int64_t, unordered_map<int64_t, unordered_set<int64_t>>> {}; // (from, to) -> keys
  auto add_dep_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
    if (info.type == EdgeType::WW) {
      if (auto e = dependency_graph.ww.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.ww.add_edge(from, to, info);
      }
    } else if (info.type == EdgeType::RW) {
      auto ins_keys = vector<int64_t> {};
      for (const auto &key : info.keys) {
        // prevent duplicated rw edges being added into dep graph
        if (rw_edges[from][to].contains(key)) continue;
        ins_keys.emplace_back(key);
      }
      if (auto e = dependency_graph.rw.edge(from, to); e) {
        copy(ins_keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.rw.add_edge(from, to, info);
      }
      rw_edges[from][to].insert(ins_keys.begin(), ins_keys.end());
    } else if (info.type == EdgeType::WR) {
      if (auto e = dependency_graph.wr.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.wr.add_edge(from, to, info);
      }
    } else {
      assert(false);
    }
  };

  // 1. prune unit wr constaint
  BOOST_LOG_TRIVIAL(trace) << "prune unit wr constraints";
  for (auto &&c : wr_constraints) {
    const auto &[key, read, writes] = c;
    if (writes.size() == 1) { // unit wr constaint
      add_dep_edge(*writes.begin(), read, EdgeInfo{
        .type = EdgeType::WR,
        .keys = {key},
      });
      pruned_wr_constraints.emplace(&c);
      BOOST_LOG_TRIVIAL(trace) << "pruned unit wr constraint, added cons: " 
                               << c;
    }
  }

  // 2. check acyclicity
  {
    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(dependency_graph.num_vertices());

      try {
        topological_sort(known_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }

      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in toposort of pruning";
      return false;
    }
  }

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;

  BOOST_LOG_TRIVIAL(debug) << "#ww constraints after pruning: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(debug) << "#wr constraints after pruning: " << wr_constraints.size();
  return true;
}

auto prune_basic_constraints(DependencyGraph &dependency_graph,
                            Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });

  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }
  auto &&i_vertex_map = unordered_map<int, int64_t>{}; // inversed vertex map
  for (auto &&[v, desc] : vertex_map) i_vertex_map[desc] = v; 
 
  auto rw_edges = unordered_map<int64_t, unordered_map<int64_t, unordered_set<int64_t>>> {}; // (from, to) -> keys
  auto add_dep_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
    if (info.type == EdgeType::WW) {
      if (auto e = dependency_graph.ww.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.ww.add_edge(from, to, info);
      }
    } else if (info.type == EdgeType::RW) {
      auto ins_keys = vector<int64_t> {};
      for (const auto &key : info.keys) {
        // prevent duplicated rw edges being added into dep graph
        if (rw_edges[from][to].contains(key)) continue;
        ins_keys.emplace_back(key);
      }
      if (auto e = dependency_graph.rw.edge(from, to); e) {
        copy(ins_keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.rw.add_edge(from, to, info);
      }
      rw_edges[from][to].insert(ins_keys.begin(), ins_keys.end());
    } else if (info.type == EdgeType::WR) {
      if (auto e = dependency_graph.wr.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.wr.add_edge(from, to, info);
      }
    } else {
      assert(false);
    }
  };

  // 1. prune unit wr constaint
  BOOST_LOG_TRIVIAL(trace) << "0. prune unit wr constraints";
  for (auto &&c : wr_constraints) {
    const auto &[key, read, writes] = c;
    if (writes.size() == 1) { // unit wr constaint
      add_dep_edge(*writes.begin(), read, EdgeInfo{
        .type = EdgeType::WR,
        .keys = {key},
      });
      pruned_wr_constraints.emplace(&c);
      BOOST_LOG_TRIVIAL(trace) << "pruned unit wr constraint, added cons: " 
                               << c;
    }
  }
  
  // 2. prepare edge visitors
  auto n = dependency_graph.num_vertices();
  auto wr_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});
  for (auto &&[from, to, info] : dependency_graph.wr.edges()) {
    auto &&keys = info.get().keys;
    for (const auto key : keys) {
      wr_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
    }
  }
  auto ww_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});

  // 3. prepare known induced rw edges
  using Edge = std::tuple<int64_t, int64_t, EdgeInfo>;
  // auto known_rw_edges_of = vector<vector<vector<Edge>>>(n, vector<vector<Edge>>(n, vector<Edge>{})); // (from, to) -> rw edges
  auto known_rw_edges_of = unordered_map<int, unordered_map<int, vector<Edge>>>{}; // (from, to) -> rw edges
  for (const auto &c : ww_constraints) {
    const auto &[either_txn_id, or_txn_id, info] = c.either_edges.at(0);
    const auto &keys = info.keys;
    auto either_ = vertex_map.at(either_txn_id);
    auto or_ = vertex_map.at(or_txn_id);

    auto induce_rw_edges = [&](int from, int to, int64_t key) -> void {
      auto rw_keys_of = unordered_map<int, unordered_map<int, set<int64_t>>>{}; // (from, to) -> keys
      auto rw_edges = vector<pair<int, int>>{};
      for (const auto &to2 : wr_to[from][key]) {
        if (to == to2) continue;
        if (rw_keys_of[to2][to].empty()) rw_edges.emplace_back(pair<int, int>{to2, to});
        rw_keys_of[to2][to].insert(key);
      }
      for (const auto &[u, v] : rw_edges) {
        const auto &keys = rw_keys_of[u][v];
        known_rw_edges_of[from][to].emplace_back(Edge{
          i_vertex_map.at(u), 
          i_vertex_map.at(v),
          EdgeInfo{
            .type = EdgeType::RW,
            .keys = vector<int64_t>(keys.begin(), keys.end()),
          },
        });
      }
    };

    for (const auto &key : keys) {
      induce_rw_edges(either_, or_, key);
      induce_rw_edges(or_, either_, key);
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "\n";
      logger << "known induced rw edges of " << either_txn_id << ", " << or_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[either_][or_]) {
        logger << "(" << from << ", " << to << "), ";
      }
      logger << "\n";
      logger << "known induced rw edges of " << or_txn_id << ", " << either_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[or_][either_]) {
        logger << "(" << from << ", " << to << "), ";
      }
    }
  }

  wr_to.assign(n, {});

  auto reachability_duration = chrono::milliseconds(0);
  auto check_duration = chrono::milliseconds(0);
  auto add_duration = chrono::milliseconds(0);
  
  bool changed = true;

  // 4. pruning passes
  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";
    changed = false;

    auto time = chrono::steady_clock::now();

    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(dependency_graph.num_vertices());

      try {
        topological_sort(known_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }

      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in toposort of pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{
          dependency_graph.num_vertices(),
          dynamic_bitset<>{dependency_graph.num_vertices()}};
      int start;
      
      auto dfs = [&](auto &self, int x) -> void {
        r.at(start).set(x);
        for (auto &&e : as_range(out_edges(x, known_graph))) {
          auto y = target(e, known_graph);
          if (!r.at(start).test(y)) self(self, y);
        }
      };

      for (auto &v : reverse_topo_order.value()) {
        start = v;
        dfs(dfs, v);
      }

      return r;
    }();

    auto can_reach = [&reachability, &vertex_map](int64_t from, int64_t to) -> bool {
      return reachability.at(vertex_map.at(from)).test(vertex_map.at(to));
    };

    {
      auto curr_time = chrono::steady_clock::now();
      reachability_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

    auto pruned_ww = unordered_map<int64_t, vector<Edge>>{}; // from -> { (from, to, edge_info) }
    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, _] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          if (reachability.at(vertex_map.at(to2)).test(vertex_map.at(from2))) {
            return false; // rw edge forms a cycle
          }
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            if (reachability.at(vertex_map.at(to)).test(to2)) {
              return false; // rw edge forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        const auto &[from, to, info] = edge;
        add_dep_edge(from, to, info); // ww edge
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, info2] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          add_dep_edge(from2, to2, info2);
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            add_dep_edge(
              i_vertex_map.at(to2), 
              to,
              EdgeInfo{
                .type = EdgeType::RW,
                .keys = {key},
              });
          }
        }
        // add edge into ww_to
        for (const auto &key : keys) {
          ww_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
        }
      };

      time = chrono::steady_clock::now();

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        // if (!can_reach(c.or_txn_id, c.either_txn_id)) {
        //   pruned_ww[c.or_txn_id].emplace_back(c.or_edges.at(0));
        // }
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
        // if (!can_reach(c.either_txn_id, c.or_txn_id)) {
        //   pruned_ww[c.either_txn_id].emplace_back(c.either_edges.at(0));
        // }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    BOOST_LOG_TRIVIAL(trace) << "skip";
  }

  BOOST_LOG_TRIVIAL(debug) << "constructing reachability time: " << reachability_duration;
  BOOST_LOG_TRIVIAL(debug) << "checking time: " << check_duration;
  BOOST_LOG_TRIVIAL(debug) << "adding edge time: " << add_duration;

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;

  BOOST_LOG_TRIVIAL(debug) << "#ww constraints after pruning: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(debug) << "#wr constraints after pruning: " << wr_constraints.size();
  return true;
}


auto fast_prune_constraints(DependencyGraph &dependency_graph,
                            Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });

  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }
  auto &&i_vertex_map = unordered_map<int, int64_t>{}; // inversed vertex map
  for (auto &&[v, desc] : vertex_map) i_vertex_map[desc] = v; 
 
  auto rw_edges = unordered_map<int64_t, unordered_map<int64_t, unordered_set<int64_t>>> {}; // (from, to) -> keys
  auto add_dep_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
    if (info.type == EdgeType::WW) {
      if (auto e = dependency_graph.ww.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.ww.add_edge(from, to, info);
      }
    } else if (info.type == EdgeType::RW) {
      auto ins_keys = vector<int64_t> {};
      for (const auto &key : info.keys) {
        // prevent duplicated rw edges being added into dep graph
        if (rw_edges[from][to].contains(key)) continue;
        ins_keys.emplace_back(key);
      }
      if (auto e = dependency_graph.rw.edge(from, to); e) {
        copy(ins_keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.rw.add_edge(from, to, info);
      }
      rw_edges[from][to].insert(ins_keys.begin(), ins_keys.end());
    } else if (info.type == EdgeType::WR) {
      if (auto e = dependency_graph.wr.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.wr.add_edge(from, to, info);
      }
    } else {
      assert(false);
    }
  };

  // 1. prune unit wr constaint
  BOOST_LOG_TRIVIAL(trace) << "0. prune unit wr constraints";
  for (auto &&c : wr_constraints) {
    const auto &[key, read, writes] = c;
    if (writes.size() == 1) { // unit wr constaint
      add_dep_edge(*writes.begin(), read, EdgeInfo{
        .type = EdgeType::WR,
        .keys = {key},
      });
      pruned_wr_constraints.emplace(&c);
      BOOST_LOG_TRIVIAL(trace) << "pruned unit wr constraint, added cons: " 
                               << c;
    }
  }
  
  // 2. prepare edge visitors
  auto n = dependency_graph.num_vertices();
  auto wr_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});
  for (auto &&[from, to, info] : dependency_graph.wr.edges()) {
    auto &&keys = info.get().keys;
    for (const auto key : keys) {
      wr_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
    }
  }
  auto ww_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});

  // 3. prepare known induced rw edges
  using Edge = std::tuple<int64_t, int64_t, EdgeInfo>;
  // auto known_rw_edges_of = vector<vector<vector<Edge>>>(n, vector<vector<Edge>>(n, vector<Edge>{})); // (from, to) -> rw edges
  auto known_rw_edges_of = unordered_map<int, unordered_map<int, vector<Edge>>>{}; // (from, to) -> rw edges
  for (const auto &c : ww_constraints) {
    const auto &[either_txn_id, or_txn_id, info] = c.either_edges.at(0);
    const auto &keys = info.keys;
    auto either_ = vertex_map.at(either_txn_id);
    auto or_ = vertex_map.at(or_txn_id);

    auto induce_rw_edges = [&](int from, int to, int64_t key) -> void {
      auto rw_keys_of = unordered_map<int, unordered_map<int, set<int64_t>>>{}; // (from, to) -> keys
      auto rw_edges = vector<pair<int, int>>{};
      for (const auto &to2 : wr_to[from][key]) {
        if (to == to2) continue;
        if (rw_keys_of[to2][to].empty()) rw_edges.emplace_back(pair<int, int>{to2, to});
        rw_keys_of[to2][to].insert(key);
      }
      for (const auto &[u, v] : rw_edges) {
        const auto &keys = rw_keys_of[u][v];
        known_rw_edges_of[from][to].emplace_back(Edge{
          i_vertex_map.at(u), 
          i_vertex_map.at(v),
          EdgeInfo{
            .type = EdgeType::RW,
            .keys = vector<int64_t>(keys.begin(), keys.end()),
          },
        });
      }
    };

    for (const auto &key : keys) {
      induce_rw_edges(either_, or_, key);
      induce_rw_edges(or_, either_, key);
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "\n";
      logger << "known induced rw edges of " << either_txn_id << ", " << or_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[either_][or_]) {
        logger << "(" << from << ", " << to << "), ";
      }
      logger << "\n";
      logger << "known induced rw edges of " << or_txn_id << ", " << either_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[or_][either_]) {
        logger << "(" << from << ", " << to << "), ";
      }
    }
  }

  wr_to.assign(n, {});

  auto reachability_duration = chrono::milliseconds(0);
  auto check_duration = chrono::milliseconds(0);
  auto add_duration = chrono::milliseconds(0);
  
  bool changed = true;

  // 4. pruning passes
  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";
    changed = false;

    auto time = chrono::steady_clock::now();

    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(dependency_graph.num_vertices());

      try {
        topological_sort(known_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }

      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in toposort of pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{
          dependency_graph.num_vertices(),
          dynamic_bitset<>{dependency_graph.num_vertices()}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_graph))) {
          auto v2 = target(e, known_graph);
          // FIXME: is this !r.at(v)[v2] condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    auto can_reach = [&reachability, &vertex_map](int64_t from, int64_t to) -> bool {
      return reachability.at(vertex_map.at(from)).test(vertex_map.at(to));
    };

    {
      auto curr_time = chrono::steady_clock::now();
      reachability_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

    auto pruned_ww = unordered_map<int64_t, vector<Edge>>{}; // from -> { (from, to, edge_info) }
    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, _] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          if (reachability.at(vertex_map.at(to2)).test(vertex_map.at(from2))) {
            return false; // rw edge forms a cycle
          }
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            if (reachability.at(vertex_map.at(to)).test(to2)) {
              return false; // rw edge forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        const auto &[from, to, info] = edge;
        add_dep_edge(from, to, info); // ww edge
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, info2] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          add_dep_edge(from2, to2, info2);
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            add_dep_edge(
              i_vertex_map.at(to2), 
              to,
              EdgeInfo{
                .type = EdgeType::RW,
                .keys = {key},
              });
          }
        }
        // add edge into ww_to
        for (const auto &key : keys) {
          ww_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
        }
      };

      time = chrono::steady_clock::now();

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        // if (!can_reach(c.or_txn_id, c.either_txn_id)) {
        //   pruned_ww[c.or_txn_id].emplace_back(c.or_edges.at(0));
        // }
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
        // if (!can_reach(c.either_txn_id, c.or_txn_id)) {
        //   pruned_ww[c.either_txn_id].emplace_back(c.either_edges.at(0));
        // }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    for (auto &&c : wr_constraints | not_pruned_wr) {
      auto check_wr_edges = [&](int64_t from, int64_t to, int64_t key) -> bool {
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // wr edge forms a cycle
        }
        // rw edges
        for (const auto &to2 : ww_to[vertex_map.at(from)][key]) {
          if (vertex_map.at(to) == unsigned(to2)) continue;
          if (reachability.at(to2).test(vertex_map.at(to))) {
            return false; // rw edge forms a cycle
          }
        }
        return true;
      };
      auto add_wr_edges = [&](int64_t from, int64_t to, int64_t key) -> void {
        // wr edge
        add_dep_edge(from, to, EdgeInfo{
          .type = EdgeType::WR,
          .keys = {key},
        }); 
        // rw edges
        for (const auto &to2 : ww_to[vertex_map.at(from)][key]) {
          if (vertex_map.at(to) == unsigned(to2)) continue;
          add_dep_edge(
            to,
            i_vertex_map.at(to2),
            EdgeInfo{
              .type = EdgeType::RW,
              .keys = {key},
            }
          );
        }
        // add (from, to, key) into wr_to
        wr_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
      };

      time = chrono::steady_clock::now();

      auto &[key, read_txn_id, write_txn_ids] = c;
      auto erased_wids = vector<int64_t>{};
      for (auto write_txn_id : write_txn_ids) {
        // direct WW optimization
        // bool erased = false;
        // for (const auto &[from, to, info] : pruned_ww[write_txn_id]) {
        //   assert(from == write_txn_id);
        //   if (write_txn_ids.contains(to) && can_reach(to, read_txn_id)) { // info.keys.contains(key)
        //     erased = true;
        //     erased_wids.emplace_back(write_txn_id);
        //     break;
        //   }
        // }
        // if (erased) continue;

        if (!check_wr_edges(write_txn_id, read_txn_id, key)) {
          erased_wids.emplace_back(write_txn_id);
        }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      for (auto id : erased_wids) write_txn_ids.erase(id);
      if (write_txn_ids.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in wr pruning:";
        CHECKER_LOG_COND(debug, logger) {
          logger << "key = " << key << "\n";
          logger << "read_txn_id = " << read_txn_id << "\n";
          logger << "write_txn_id = {";
          for (auto id : erased_wids) {
            logger << id << ", ";
          }
          logger << "}\n";
        }
        return false;
      }

      auto pruned = false;
      if (write_txn_ids.size() == 1) {
        pruned = changed = true;

        time = chrono::steady_clock::now();

        add_wr_edges(*write_txn_ids.begin(), read_txn_id, key);

        {
          auto curr_time = chrono::steady_clock::now();
          add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
          time = curr_time;
        }
      }

      if (pruned) {
        pruned_wr_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned wr constraint, added cons: " 
                                 << c;
      }
    }
    
    malloc_trim(0);
  }

  BOOST_LOG_TRIVIAL(debug) << "constructing reachability time: " << reachability_duration;
  BOOST_LOG_TRIVIAL(debug) << "checking time: " << check_duration;
  BOOST_LOG_TRIVIAL(debug) << "adding edge time: " << add_duration;

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;

  BOOST_LOG_TRIVIAL(debug) << "#ww constraints after pruning: " << ww_constraints.size();
  BOOST_LOG_TRIVIAL(debug) << "#wr constraints after pruning: " << wr_constraints.size();
  return true;
}

auto prune_constraints(DependencyGraph &dependency_graph,
                       Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;
  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });
  bool changed = true;

  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }

  auto reachability_duration = chrono::milliseconds(0);
  auto check_duration = chrono::milliseconds(0);
  auto add_duration = chrono::milliseconds(0);

  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";

    changed = false;

    auto time = chrono::steady_clock::now();

    auto known_graph = adjacency_list<>{dependency_graph.num_vertices()};
    for (auto &&[from, to, _] : dependency_graph.edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), known_graph);
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(dependency_graph.num_vertices());

      try {
        topological_sort(known_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }

      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{
          dependency_graph.num_vertices(),
          dynamic_bitset<>{dependency_graph.num_vertices()}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_graph))) {
          auto v2 = target(e, known_graph);
          // FIXME: is this !r.at(v)[v2] condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    {
      auto curr_time = chrono::steady_clock::now();
      reachability_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

    auto keys_intersection = [](const vector<int64_t> &v1, const vector<int64_t> &v2) -> vector<int64_t> {
      auto s1 = set<int64_t>(v1.begin(), v1.end()), s2 = set<int64_t>(v2.begin(), v2.end());
      auto s3 = set<int64_t>{};
      std::set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(s3, s3.end()));
      return vector(s3.begin(), s3.end());
    };
    auto add_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
      switch (info.type) {
        case EdgeType::WW:
          if (auto e = dependency_graph.ww.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.ww.add_edge(from, to, info);
          }
          break;
        case EdgeType::RW:
          if (auto e = dependency_graph.rw.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.rw.add_edge(from, to, info);
          }
          break;
        case EdgeType::WR:
          if (auto e = dependency_graph.wr.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.wr.add_edge(from, to, info);
          }
          break;
        default:
          assert(false);
      }
    };

    // 1. check ww constraints
    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (!keys_intersection(e.value().get().keys, info.keys).empty()) {
            if (reachability.at(vertex_map.at(to)).test(vertex_map.at(v))) {
              return false; // rw edge(v, to) forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        auto &[from, to, info] = edge;
        add_edge(from, to, info); // ww edge
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (v, to)
            add_edge(v, to, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      time = chrono::steady_clock::now();

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        pruned = true;
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
      }

      {
        auto curr_time = chrono::steady_clock::now();
        add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    // 2. check wr constraints
    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    for (auto &&c : wr_constraints | not_pruned_wr) {
      auto check_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> bool {
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // wr edge forms a cycle
        }
        // rw edges
        for (auto v : dependency_graph.ww.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            if (reachability.at(vertex_map.at(v)).test(vertex_map.at(to))) {
              return false; // rw edge (to, v) forms a cycle
            }
            // we should check (from, v), for theres a trail of from -> to -> v , 
            // however, from -> v is a WW edge, so no need
          }
        }
        return true;
      };
      auto add_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
        add_edge(from, to, info); // wr edge
        for (auto v : dependency_graph.ww.successors(from)) {
          if (to == v) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (to, v)
            add_edge(to, v, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      time = chrono::steady_clock::now();

      auto &[key, read_txn_id, write_txn_ids] = c;
      auto erased_wids = vector<int64_t>{};
      for (auto write_txn_id : write_txn_ids) {
        if (!check_wr_edges(write_txn_id, read_txn_id, EdgeInfo{
            .type = EdgeType::WR,
            .keys = vector<int64_t>{key},
          })) {
          erased_wids.emplace_back(write_txn_id);
        }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      for (auto id : erased_wids) write_txn_ids.erase(id);
      if (write_txn_ids.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in wr pruning:";
        CHECKER_LOG_COND(debug, logger) {
          logger << "key = " << key << "\n";
          logger << "read_txn_id = " << read_txn_id << "\n";
          logger << "write_txn_id = {";
          for (auto id : erased_wids) {
            logger << id << ", ";
          }
          logger << "}\n";
        }
        return false;
      }
      auto pruned = false;
      if (write_txn_ids.size() == 1) {
        pruned = changed = true;

        time = chrono::steady_clock::now();

        add_wr_edges(*write_txn_ids.begin(), read_txn_id, EdgeInfo{
          .type = EdgeType::WR,
          .keys = vector<int64_t>{key},
        });

        {
          auto curr_time = chrono::steady_clock::now();
          add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
          time = curr_time;
        }
      }

      if (pruned) {
        pruned_wr_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned wr constraint, added edges:" 
                                 << c;
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "constructing reachability time: " << reachability_duration;
  BOOST_LOG_TRIVIAL(debug) << "checking time: " << check_duration;
  BOOST_LOG_TRIVIAL(debug) << "adding edge time: " << add_duration;

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;
  return true;
}

auto fast_prune_si_constraints(DependencyGraph &dependency_graph,
                            Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });

  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }
  auto &&i_vertex_map = unordered_map<int, int64_t>{}; // inversed vertex map
  for (auto &&[v, desc] : vertex_map) i_vertex_map[desc] = v; 
 
  auto rw_edges = unordered_map<int64_t, unordered_map<int64_t, unordered_set<int64_t>>> {}; // (from, to) -> keys
  auto add_dep_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
    if (info.type == EdgeType::WW) {
      if (auto e = dependency_graph.ww.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.ww.add_edge(from, to, info);
      }
    } else if (info.type == EdgeType::RW) {
      auto ins_keys = vector<int64_t> {};
      for (const auto &key : info.keys) {
        // prevent duplicated rw edges being added into dep graph
        if (rw_edges[from][to].contains(key)) continue;
        ins_keys.emplace_back(key);
      }
      if (auto e = dependency_graph.rw.edge(from, to); e) {
        copy(ins_keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.rw.add_edge(from, to, info);
      }
      rw_edges[from][to].insert(ins_keys.begin(), ins_keys.end());
    } else if (info.type == EdgeType::WR) {
      if (auto e = dependency_graph.wr.edge(from, to); e) {
        copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        dependency_graph.wr.add_edge(from, to, info);
      }
    } else {
      assert(false);
    }
  };

  // 1. prune unit wr constaint
  BOOST_LOG_TRIVIAL(trace) << "0. prune unit wr constraints";
  for (auto &&c : wr_constraints) {
    const auto &[key, read, writes] = c;
    if (writes.size() == 1) { // unit wr constaint
      add_dep_edge(*writes.begin(), read, EdgeInfo{
        .type = EdgeType::WR,
        .keys = {key},
      });
      pruned_wr_constraints.emplace(&c);
      BOOST_LOG_TRIVIAL(trace) << "pruned unit wr constraint, added cons: " 
                               << c;
    }
  }
  
  // 2. prepare edge visitors
  auto n = dependency_graph.num_vertices();
  auto wr_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});
  for (auto &&[from, to, info] : dependency_graph.wr.edges()) {
    auto &&keys = info.get().keys;
    for (const auto key : keys) {
      wr_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
    }
  }
  auto ww_to = vector<unordered_map<int64_t, vector<int>>> (n, unordered_map<int64_t, vector<int>>{});

  // 3. prepare known induced rw edges
  using Edge = std::tuple<int64_t, int64_t, EdgeInfo>;
  // auto known_rw_edges_of = vector<vector<vector<Edge>>>(n, vector<vector<Edge>>(n, vector<Edge>{})); // (from, to) -> rw edges
  auto known_rw_edges_of = unordered_map<int, unordered_map<int, vector<Edge>>>{}; // (from, to) -> rw edges
  for (const auto &c : ww_constraints) {
    const auto &[either_txn_id, or_txn_id, info] = c.either_edges.at(0);
    const auto &keys = info.keys;
    auto either_ = vertex_map.at(either_txn_id);
    auto or_ = vertex_map.at(or_txn_id);

    auto induce_rw_edges = [&](int from, int to, int64_t key) -> void {
      auto rw_keys_of = unordered_map<int, unordered_map<int, set<int64_t>>>{}; // (from, to) -> keys
      auto rw_edges = vector<pair<int, int>>{};
      for (const auto &to2 : wr_to[from][key]) {
        if (to == to2) continue;
        if (rw_keys_of[to2][to].empty()) rw_edges.emplace_back(pair<int, int>{to2, to});
        rw_keys_of[to2][to].insert(key);
      }
      for (const auto &[u, v] : rw_edges) {
        const auto &keys = rw_keys_of[u][v];
        known_rw_edges_of[from][to].emplace_back(Edge{
          i_vertex_map.at(u), 
          i_vertex_map.at(v),
          EdgeInfo{
            .type = EdgeType::RW,
            .keys = vector<int64_t>(keys.begin(), keys.end()),
          },
        });
      }
    };

    for (const auto &key : keys) {
      induce_rw_edges(either_, or_, key);
      induce_rw_edges(or_, either_, key);
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "\n";
      logger << "known induced rw edges of " << either_txn_id << ", " << or_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[either_][or_]) {
        logger << "(" << from << ", " << to << "), ";
      }
      logger << "\n";
      logger << "known induced rw edges of " << or_txn_id << ", " << either_txn_id << ": ";
      for (const auto &[from, to, _] : known_rw_edges_of[or_][either_]) {
        logger << "(" << from << ", " << to << "), ";
      }
    }
  }

  wr_to.assign(n, {});

  auto reachability_duration = chrono::milliseconds(0);
  auto check_duration = chrono::milliseconds(0);
  auto add_duration = chrono::milliseconds(0);
  
  bool changed = true;

  // 4. pruning passes
  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";
    changed = false;

    auto time = chrono::steady_clock::now();

    auto dep_graph = adjacency_list<>{n}, anti_dep_graph = adjacency_list<>{n}, known_induced_graph = adjacency_list<>{n};
    for (const auto &&[from, to, _] : dependency_graph.dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), dep_graph);
      add_edge(vertex_map.at(from), vertex_map.at(to), known_induced_graph);
    }
    for (const auto &&[from, to, _] : dependency_graph.anti_dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), anti_dep_graph);
    }
    for (const auto &from : as_range(vertices(dep_graph))) {
      for (const auto &&from_edge : as_range(out_edges(from, dep_graph))) {
        const auto middle = target(from_edge, dep_graph);
        for (const auto &&to_edge : as_range(out_edges(middle, anti_dep_graph))) {
          const auto to = target(to_edge, anti_dep_graph);
          add_edge(from, to, known_induced_graph);
        }
      }
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "dep_graph:\n";
      for (const auto &&e : as_range(edges(dep_graph))) {
        auto from = source(e, dep_graph), to = target(e, dep_graph);
        logger << from << " " << to << "\n"; 
      }
      logger << "antidep_graph:\n";
      for (const auto &&e : as_range(edges(anti_dep_graph))) {
        auto from = source(e, anti_dep_graph), to = target(e, anti_dep_graph);
        logger << from << " " << to << "\n"; 
      }
      logger << "known_induced_graph:\n";
      for (const auto &&e : as_range(edges(known_induced_graph))) {
        auto from = source(e, known_induced_graph), to = target(e, known_induced_graph);
        logger << from << " " << to << "\n"; 
      }
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(n);
      try {
        topological_sort(known_induced_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }
      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_induced_graph))) {
          auto v2 = target(e, known_induced_graph);
          // FIXME: is this (!r.at(v)[v2]) condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    auto pred_edges = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};
    for (const auto &&edge : as_range(edges(dep_graph))) {
      auto from = source(edge, dep_graph), to = target(edge, dep_graph);
      pred_edges.at(to).set(from);
    }

    {
      auto curr_time = chrono::steady_clock::now();
      reachability_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, _] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          // SER pruning of RW edges:
          // if (reachability.at(vertex_map.at(to2)).test(vertex_map.at(from2))) {
          //   return false; // rw edge (from2, to2) forms a cycle
          // }
          // SI:
          if ((pred_edges.at(vertex_map.at(from2)) & reachability.at(vertex_map.at(to2))).any()) {
            return false; // rw edge (from2, to2) forms a cycle
          }
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            // SER pruning of RW edges:
            // if (reachability.at(vertex_map.at(to)).test(to2)) {
            //   return false; // rw edge (to2, to) forms a cycle
            // }
            // SI:
            if ((pred_edges.at(to2) & reachability.at(vertex_map.at(to))).any()) { // to2 is already mapped by vertex_map
              return false; // rw edge (to2, to) forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        const auto &[from, to, info] = edge;
        add_dep_edge(from, to, info); // ww edge
        // rw edges part 1. known induced rw edges
        for (const auto &[from2, to2, info2] : known_rw_edges_of[vertex_map.at(from)][vertex_map.at(to)]) {
          add_dep_edge(from2, to2, info2);
        }
        // rw edges part 2. new induced rw edges
        const auto &keys = info.keys;
        for (const auto &key : keys) {
          for (const auto &to2 : wr_to[vertex_map.at(from)][key]) {
            if (vertex_map.at(to) == unsigned(to2)) continue;
            add_dep_edge(
              i_vertex_map.at(to2), 
              to,
              EdgeInfo{
                .type = EdgeType::RW,
                .keys = {key},
              });
          }
        }
        // add edge into ww_to
        for (const auto &key : keys) {
          ww_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
        }
      };

      time = chrono::steady_clock::now();

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        pruned = true;
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
      }

      {
        auto curr_time = chrono::steady_clock::now();
        add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    for (auto &&c : wr_constraints | not_pruned_wr) {
      auto check_wr_edges = [&](int64_t from, int64_t to, int64_t key) -> bool {
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // wr edge forms a cycle
        }
        // rw edges
        for (const auto &to2 : ww_to[vertex_map.at(from)][key]) {
          if (vertex_map.at(to) == unsigned(to2)) continue;
          // SER pruning of RW edges:
          // if (reachability.at(to2).test(vertex_map.at(to))) {
          //   return false; // rw edge (to, to2) forms a cycle
          // }
          // SI:
          if ((pred_edges.at(vertex_map.at(to)) & reachability.at(to2)).any()) { // to2 is already mapped by vertex_map
            return false; // rw edge (to, to2) forms a cycle
          }
        }
        return true;
      };
      auto add_wr_edges = [&](int64_t from, int64_t to, int64_t key) -> void {
        // wr edge
        add_dep_edge(from, to, EdgeInfo{
          .type = EdgeType::WR,
          .keys = {key},
        }); 
        // rw edges
        for (const auto &to2 : ww_to[vertex_map.at(from)][key]) {
          if (vertex_map.at(to) == unsigned(to2)) continue;
          add_dep_edge(
            to,
            i_vertex_map.at(to2),
            EdgeInfo{
              .type = EdgeType::RW,
              .keys = {key},
            }
          );
        }
        // add (from, to, key) into wr_to
        wr_to[vertex_map.at(from)][key].emplace_back(vertex_map.at(to));
      };

      time = chrono::steady_clock::now();

      auto &[key, read_txn_id, write_txn_ids] = c;
      auto erased_wids = vector<int64_t>{};
      for (auto write_txn_id : write_txn_ids) {
        if (!check_wr_edges(write_txn_id, read_txn_id, key)) {
          erased_wids.emplace_back(write_txn_id);
        }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      for (auto id : erased_wids) write_txn_ids.erase(id);
      if (write_txn_ids.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in wr pruning:";
        CHECKER_LOG_COND(debug, logger) {
          logger << "key = " << key << "\n";
          logger << "read_txn_id = " << read_txn_id << "\n";
          logger << "write_txn_id = {";
          for (auto id : erased_wids) {
            logger << id << ", ";
          }
          logger << "}\n";
        }
        return false;
      }

      auto pruned = false;
      if (write_txn_ids.size() == 1) {
        pruned = changed = true;

        time = chrono::steady_clock::now();

        add_wr_edges(*write_txn_ids.begin(), read_txn_id, key);

        {
          auto curr_time = chrono::steady_clock::now();
          add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
          time = curr_time;
        }
      }

      if (pruned) {
        pruned_wr_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned wr constraint, added cons: " 
                                 << c;
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "constructing reachability time: " << reachability_duration;
  BOOST_LOG_TRIVIAL(debug) << "checking time: " << check_duration;
  BOOST_LOG_TRIVIAL(debug) << "adding edge time: " << add_duration;

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;
  return true;
}

auto prune_si_constraints(DependencyGraph &dependency_graph,
                       Constraints &constraints) -> bool {
  auto &[ww_constraints, wr_constraints] = constraints;

  auto n = dependency_graph.num_vertices();
  auto &&vertex_map = dependency_graph.rw.vertex_map.left;
  auto pruned_ww_constraints = unordered_set<WWConstraint *>{};
  auto not_pruned_ww = filter([&](auto &&c) { return !pruned_ww_constraints.contains(&c); });
  auto pruned_wr_constraints = unordered_set<WRConstraint *>{};
  auto not_pruned_wr = filter([&](auto &&c) { return !pruned_wr_constraints.contains(&c); });
  bool changed = true;

  CHECKER_LOG_COND(trace, logger) {
    logger << "vertex_map:";
    for (auto &&[v, desc] : vertex_map) {
      logger << " (" << v << ", " << desc << ')';
    }
  }

  auto reachability_duration = chrono::milliseconds(0);
  auto check_duration = chrono::milliseconds(0);
  auto add_duration = chrono::milliseconds(0);

  while (changed) {
    BOOST_LOG_TRIVIAL(trace) << "new pruning pass:";

    changed = false;

    auto time = chrono::steady_clock::now();

    auto dep_graph = adjacency_list<>{n}, anti_dep_graph = adjacency_list<>{n}, known_induced_graph = adjacency_list<>{n};
    for (const auto &&[from, to, _] : dependency_graph.dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), dep_graph);
      add_edge(vertex_map.at(from), vertex_map.at(to), known_induced_graph);
    }
    for (const auto &&[from, to, _] : dependency_graph.anti_dep_edges()) {
      add_edge(vertex_map.at(from), vertex_map.at(to), anti_dep_graph);
    }
    for (const auto &from : as_range(vertices(dep_graph))) {
      for (const auto &&from_edge : as_range(out_edges(from, dep_graph))) {
        const auto middle = target(from_edge, dep_graph);
        for (const auto &&to_edge : as_range(out_edges(middle, anti_dep_graph))) {
          const auto to = target(to_edge, anti_dep_graph);
          add_edge(from, to, known_induced_graph);
        }
      }
    }

    CHECKER_LOG_COND(trace, logger) {
      logger << "dep_graph:\n";
      for (const auto &&e : as_range(edges(dep_graph))) {
        auto from = source(e, dep_graph), to = target(e, dep_graph);
        logger << from << " " << to << "\n"; 
      }
      logger << "antidep_graph:\n";
      for (const auto &&e : as_range(edges(anti_dep_graph))) {
        auto from = source(e, anti_dep_graph), to = target(e, anti_dep_graph);
        logger << from << " " << to << "\n"; 
      }
      logger << "known_induced_graph:\n";
      for (const auto &&e : as_range(edges(known_induced_graph))) {
        auto from = source(e, known_induced_graph), to = target(e, known_induced_graph);
        logger << from << " " << to << "\n"; 
      }
    }

    auto reverse_topo_order =
        [&]() -> optional<vector<adjacency_list<>::vertex_descriptor>> {
      auto v = vector<adjacency_list<>::vertex_descriptor>(n);
      try {
        topological_sort(known_induced_graph, v.begin(), no_named_parameters());
      } catch (not_a_dag &e) {
        return nullopt;
      }
      return {std::move(v)};
    }();

    if (!reverse_topo_order) {
      BOOST_LOG_TRIVIAL(debug) << "conflict found in pruning";
      return false;
    }

    auto reachability = [&]() {
      auto r = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};

      for (auto &&v : reverse_topo_order.value()) {
        r.at(v).set(v);

        for (auto &&e : as_range(out_edges(v, known_induced_graph))) {
          auto v2 = target(e, known_induced_graph);
          // FIXME: is this (!r.at(v)[v2]) condition sufficient to this dynamic programming? 
          // if (!r.at(v)[v2]) {
          //   r.at(v) |= r.at(v2);
          // }
          r.at(v) |= r.at(v2);
        }
      }

      return r;
    }();

    auto pred_edges = vector<dynamic_bitset<>>{n, dynamic_bitset<>{n}};
    for (const auto &&edge : as_range(edges(dep_graph))) {
      auto from = source(edge, dep_graph), to = target(edge, dep_graph);
      pred_edges.at(to).set(from);
    }

    {
      auto curr_time = chrono::steady_clock::now();
      reachability_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
      time = curr_time;
    }

    auto keys_intersection = [](const vector<int64_t> &v1, const vector<int64_t> &v2) -> vector<int64_t> {
      auto s1 = set<int64_t>(v1.begin(), v1.end()), s2 = set<int64_t>(v2.begin(), v2.end());
      auto s3 = set<int64_t>{};
      std::set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(), std::inserter(s3, s3.end()));
      return vector(s3.begin(), s3.end());
    };

    auto add_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
      switch (info.type) {
        case EdgeType::WW:
          if (auto e = dependency_graph.ww.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.ww.add_edge(from, to, info);
          }
          break;
        case EdgeType::RW:
          if (auto e = dependency_graph.rw.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.rw.add_edge(from, to, info);
          }
          break;
        case EdgeType::WR:
          if (auto e = dependency_graph.wr.edge(from, to); e) {
            copy(info.keys, back_inserter(e.value().get().keys));
          } else {
            dependency_graph.wr.add_edge(from, to, info);
          }
          break;
        default:
          assert(false);
      }
    };

    // 1. check ww constraints
    BOOST_LOG_TRIVIAL(trace) << "1. check ww constraints:";
    for (auto &&c : ww_constraints | not_pruned_ww) {
      auto check_ww_edges = [&](WWConstraint::Edge &edge) -> bool {
        const auto &[from, to, info] = edge;
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // ww edge forms a cycle
        }
        // rw edges
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (!keys_intersection(e.value().get().keys, info.keys).empty()) {
            // SER pruning of RW edges:
            // if (reachability.at(vertex_map.at(to)).test(vertex_map.at(v))) {
            //   return false; // rw edge(v, to) forms a cycle
            // }
            // SI:
            if ((pred_edges.at(vertex_map.at(v)) & reachability.at(vertex_map.at(to))).any()) {
              return false; // rw edge(v, to) forms a cycle
            }
          }
        }
        return true;
      };
      auto add_ww_edges = [&](WWConstraint::Edge &edge) -> void {
        auto &[from, to, info] = edge;
        add_edge(from, to, info); // ww edge
        for (auto &v : dependency_graph.wr.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.wr.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (v, to)
            add_edge(v, to, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      time = chrono::steady_clock::now();

      auto add_or = !check_ww_edges(c.either_edges.at(0));
      auto add_either = !check_ww_edges(c.or_edges.at(0));
      if (add_or && add_either) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in ww pruning";
        return false;
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      auto added_edges_name = "or";
      auto pruned = false;
      if (add_or) {
        add_ww_edges(c.or_edges.at(0));
        changed = pruned = true;
        pruned = true;
      } else if (add_either) {
        add_ww_edges(c.either_edges.at(0));
        changed = pruned = true;
        added_edges_name = "either";
      }

      {
        auto curr_time = chrono::steady_clock::now();
        add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      if (pruned) {
        pruned_ww_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned ww constraint, added "
                                 << added_edges_name << " edges: " << c;
      }
    }

    // 2. check wr constraints
    BOOST_LOG_TRIVIAL(trace) << "2. check wr constraints:";
    for (auto &&c : wr_constraints | not_pruned_wr) {
      auto check_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> bool {
        if (reachability.at(vertex_map.at(to)).test(vertex_map.at(from))) {
          return false; // wr edge forms a cycle
        }
        // rw edges
        for (auto v : dependency_graph.ww.successors(from)) {
          if (v == to) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            if ((pred_edges.at(vertex_map.at(to)) & reachability.at(vertex_map.at(v))).any()) {
              return false; // rw edge (to, v) forms a cycle
            }
            // we should check (from, v), for there's a trail of from -> to -> v , 
            // however, from -> v is a WW edge, so no need
          }
        }
        return true;
      };
      auto add_wr_edges = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
        add_edge(from, to, info); // wr edge
        for (auto v : dependency_graph.ww.successors(from)) {
          if (to == v) continue;
          auto e = dependency_graph.ww.edge(from, v);
          assert(e);
          if (auto keys = keys_intersection(e.value().get().keys, info.keys); !keys.empty()) {
            // rw edge (to, v)
            add_edge(to, v, EdgeInfo{
              .type = EdgeType::RW,
              .keys = keys,
            });
          }
        }
      };

      time = chrono::steady_clock::now();

      auto &[key, read_txn_id, write_txn_ids] = c;
      auto erased_wids = vector<int64_t>{};
      for (auto write_txn_id : write_txn_ids) {
        if (!check_wr_edges(write_txn_id, read_txn_id, EdgeInfo{
            .type = EdgeType::WR,
            .keys = vector<int64_t>{key},
          })) {
          erased_wids.emplace_back(write_txn_id);
        }
      }

      {
        auto curr_time = chrono::steady_clock::now();
        check_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
        time = curr_time;
      }

      for (auto id : erased_wids) write_txn_ids.erase(id);
      if (write_txn_ids.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "conflict found in wr pruning:";
        CHECKER_LOG_COND(debug, logger) {
          logger << "key = " << key << "\n";
          logger << "read_txn_id = " << read_txn_id << "\n";
          logger << "write_txn_id = {";
          for (auto id : erased_wids) {
            logger << id << ", ";
          }
          logger << "}\n";
        }
        return false;
      }
      auto pruned = false;
      if (write_txn_ids.size() == 1) {
        pruned = changed = true;

        time = chrono::steady_clock::now();

        add_wr_edges(*write_txn_ids.begin(), read_txn_id, EdgeInfo{
          .type = EdgeType::WR,
          .keys = vector<int64_t>{key},
        });

        {
          auto curr_time = chrono::steady_clock::now();
          add_duration += chrono::duration_cast<chrono::milliseconds>(curr_time - time);
          time = curr_time;
        }
      }

      if (pruned) {
        pruned_wr_constraints.emplace(&c);
        BOOST_LOG_TRIVIAL(trace) << "pruned wr constraint, added edges:" 
                                 << c;
      }
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "constructing reachability time: " << reachability_duration;
  BOOST_LOG_TRIVIAL(debug) << "checking time: " << check_duration;
  BOOST_LOG_TRIVIAL(debug) << "adding edge time: " << add_duration;

  ww_constraints = ww_constraints | not_pruned_ww | to<vector<WWConstraint>>;
  wr_constraints = wr_constraints | not_pruned_wr | to<vector<WRConstraint>>;
  return true;
}

}  // namespace checker::solver
