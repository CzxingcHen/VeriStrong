#include "dependencygraph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/lookup_edge.hpp>
#include <cstdint>
#include <ostream>
#include <ranges>
#include <syncstream>
#include <unordered_map>
#include <utility>

#include "history.h"
#include "utils/literal.h"
#include "utils/log.h"

using boost::add_edge;
using boost::add_vertex;
using std::pair;
using std::unordered_map;
using std::ranges::views::filter;

namespace checker::history {

auto known_graph_of(const History &history) -> DependencyGraph {
  auto graph = DependencyGraph{};
  auto transactions = unordered_map<int64_t, const Transaction *>{};

  for (const auto &txn : history.transactions()) {
    transactions.emplace(txn.id, &txn);

    for (auto subgraph : {&graph.rw, &graph.so, &graph.wr, &graph.ww}) {
      subgraph->add_vertex(txn.id);
    }
  }

  // add SO edges
  for (const auto &sess : history.sessions) {
    auto prev_txn = (const Transaction *){};
    for (const auto &txn : sess.transactions) {
      if (prev_txn) {
        graph.so.add_edge(prev_txn->id, txn.id, EdgeInfo{.type = EdgeType::SO});
      }

      prev_txn = &txn;
    }
  }
  return graph; // if UniqueValue constraint is relaxed, known graph only constains SO edges.

  // add WR edges
  // auto hash_pair = [](const pair<int64_t, int64_t> &p) {
  //   return std::hash<int64_t>{}(p.first) ^ std::hash<int64_t>{}(p.second);
  // };
  // auto writes = std::unordered_map<pair<int64_t, int64_t>, const Transaction *,
  //                                  decltype(hash_pair)>{};
  // auto filter_by_type = [](EventType t) {
  //   return filter([=](const auto &ev) { return ev.type == t; });
  // };

  // for (const auto &ev : history.events() | filter_by_type(EventType::WRITE)) {
  //   writes.try_emplace({ev.key, ev.value}, transactions[ev.transaction_id]);
  // }

  // for (const auto &ev : history.events() | filter_by_type(EventType::READ)) {
  //   auto write_txn = writes[{ev.key, ev.value}];
  //   auto txn = transactions.at(ev.transaction_id);

  //   if (write_txn == txn) {
  //     continue;
  //   }

  //   if (auto edge = graph.wr.edge(write_txn->id, txn->id); edge) {
  //     edge.value().get().keys.emplace_back(ev.key);
  //   } else {
  //     graph.wr.add_edge(
  //         write_txn->id, txn->id,
  //         EdgeInfo{.type = EdgeType::WR, .keys = std::vector{ev.key}});
  //   }
  // }

  // return graph;
}

auto instrument_known_ww(const History &history, DependencyGraph &known_graph, const std::vector<std::tuple<int64_t, int64_t, int64_t>> &known_ww) -> bool {
  auto add_dep_ww_edge = [&](int64_t from, int64_t to, EdgeInfo info) -> void {
    if (info.type == EdgeType::WW) {
      if (auto e = known_graph.ww.edge(from, to); e) {
        std::ranges::copy(info.keys, back_inserter(e.value().get().keys));
      } else {
        known_graph.ww.add_edge(from, to, info);
      }
    } else {
      assert(false);
    }
  };
  auto txn_of_key_value = unordered_map<int64_t, unordered_map<int64_t, int64_t>>{}; // key -> (value -> txn_id)
  for (const auto &[key, value, type, txn_id] : history.events()) {
    if (type == EventType::READ) continue;
    if (txn_of_key_value[key][value] != 0) return false; // reject non-uniquevalue histories
    txn_of_key_value[key][value] = txn_id;
  }

  auto write_order_per_txn = unordered_map<int64_t, unordered_map<int64_t, unordered_map<int64_t, int64_t>>> {};
  for (const auto &txn : history.transactions()) {
    int64_t cnt = 0;
    for (const auto &[key, value, type, txn_id] : txn.events) {
      ++cnt;
      write_order_per_txn[txn_id][key][value] = cnt;
    }
  }

  int known_ww_cnt = 0;
  for (const auto &[key, value1, value2] : known_ww) {
    if (value1 == value2) return false; // impossible read
    auto txn1 = txn_of_key_value[key][value1];
    auto txn2 = txn_of_key_value[key][value2];
    if (txn1 == txn2) {
      auto write_order1 = write_order_per_txn[txn1][key][value1];
      auto write_order2 = write_order_per_txn[txn1][key][value2];
      if (write_order2 < write_order1) return false; // violate INT axiom
      continue;
    }
    add_dep_ww_edge(txn1, txn2, (EdgeInfo) { .type = EdgeType::WW, .keys = {key} });
    ++known_ww_cnt;
  }
  BOOST_LOG_TRIVIAL(debug) << "#known_ww: " << known_ww_cnt;
  return true;
}

auto operator<<(std::ostream &os, const EdgeInfo &edge_info) -> std::ostream & {
  auto out = std::osyncstream{os};
  auto print_keys = [&] {
    out << '(';

    const auto &keys = edge_info.keys;
    for (auto i = 0_uz; i < keys.size(); i++) {
      out << keys.at(i);
      if (i != keys.size() - 1) {
        out << ' ';
      }
    }

    out << ')';
  };

  switch (edge_info.type) {
    case EdgeType::WW:
      out << "WW";
      print_keys();
      break;
    case EdgeType::WR:
      out << "WR";
      print_keys();
      break;
    case EdgeType::RW:
      out << "RW";
      print_keys();
      break;
    case EdgeType::SO:
      out << "SO";
      break;
  }

  return os;
}

auto operator<<(std::ostream &os, const DependencyGraph &graph)
    -> std::ostream & {
  auto out = std::osyncstream{os};
  out << "RW:\n"
      << graph.rw << '\n'
      << "WW:\n"
      << graph.ww << '\n'
      << "SO:\n"
      << graph.so << '\n'
      << "WR:\n"
      << graph.wr << '\n';

  return os;
}
}  // namespace checker::history
