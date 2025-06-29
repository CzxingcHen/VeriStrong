#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <iostream>

#include <acyclic_minisat.h>
#include <acyclic_minisat_si.h>

#include "acyclicMinisatSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/agnf.h"
#include "utils/subprocess.hpp"
#include "utils/tarjan_graph.h"

namespace fs = std::filesystem;

using EdgeType = checker::history::EdgeType;

namespace checker::solver {

AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const history::Constraints &constraints,
                                           const history::HistoryMetaInfo &history_meta_info,
                                           const std::string &isolation_level) {
  target_isolation_level = isolation_level;

  // 0. touch n_vertices
  n_vertices = known_graph.num_vertices();

  auto node_id = std::unordered_map<int64_t, int>{};
  auto nodes_cnt = 0;
  auto remap = [&node_id, &nodes_cnt](int64_t x) -> int {
    if (node_id.contains(x)) return node_id.at(x);
    return node_id[x] = nodes_cnt++;
  };

  // 1. construct am_known_graph
  for (const auto &[from, to, info] : known_graph.edges()) {
    int t = -1;
    switch (info.get().type) {
      case EdgeType::SO : t = 0; break;
      case EdgeType::WW : t = 1; break;
      case EdgeType::WR : t = 2; break;
      case EdgeType::RW : t = 3; break;
      default: assert(false);
    }
    assert(t != -1);
    am_known_graph.emplace_back(AMEdge{t, remap(from), remap(to), info.get().keys});
  }

  // 2. construct am_constraints
  const auto &[ww_cons, wr_cons] = constraints;
  auto &[am_ww_cons, am_wr_cons] = am_constraints;
  // 2.1 construct ww_constraints
  for (const auto &[either_, or_, either_edges, or_edges] : ww_cons) {
    am_ww_cons.emplace_back(AMWWConstraint{remap(either_), remap(or_), get<2>(either_edges.at(0)).keys}); // edge info
    // assert(either_edges.info.keys == or_edges.info.keys);
  }
  // 2.2 construct wr_constraints
  for (const auto &[key, read_txn_id, write_txn_ids] : wr_cons) {
    auto new_write_txn_ids = std::vector<int>();
    std::transform(write_txn_ids.begin(), write_txn_ids.end(),
                   std::back_inserter(new_write_txn_ids),
                   remap);
    am_wr_cons.emplace_back(AMWRConstraint{remap(read_txn_id), new_write_txn_ids, key});
  }

  CHECKER_LOG_COND(trace, logger) {
    logger << "node map: ";
    for (const auto &[k, v] : node_id) {
      logger << "(" << k << ", " << v << "), ";
    }
    logger << "\n";
  }
  
  // 3. copy history meta info
  n_sessions = history_meta_info.n_sessions;
  n_total_transactions = history_meta_info.n_total_transactions;
  n_total_events = history_meta_info.n_total_events;
  
  auto map_composite = [](
    const std::unordered_map<int64_t, int> &node_id, 
    const std::unordered_map<int64_t, std::unordered_map<int64_t, int>> &steps_map
    ) -> std::unordered_map<int, std::unordered_map<int64_t, int>> {
    auto result_map = std::unordered_map<int, std::unordered_map<int64_t, int>>{};
    for (const auto &[raw_txn_id, rest_map] : steps_map) {
      result_map[node_id.at(raw_txn_id)] = rest_map;
    }
    return result_map;
  };
  // TODO: efficiency concerning of map_composite
  write_steps = map_composite(node_id, history_meta_info.write_steps);
  read_steps = map_composite(node_id, history_meta_info.read_steps);

  for (const auto &[txn, dist] : history_meta_info.txn_distance) {
    txn_distance.emplace(node_id.at(txn), dist);
  }

  auto sess_id = std::unordered_map<int64_t, int>{};
  auto sess_id_cnt = 0;
  auto remap_sess = [&sess_id, &sess_id_cnt](int64_t raw_session_id) -> int {
    if (!sess_id.contains(raw_session_id)) sess_id[raw_session_id] = sess_id_cnt++;
    return sess_id.at(raw_session_id);
  };

  for (const auto &[txn_id, session_id] : history_meta_info.session_id_of) {
    session_id_of.emplace(node_id.at(txn_id), remap_sess(session_id));
  }
}

/*
AcyclicMinisatSolver::AcyclicMinisatSolver(const history::DependencyGraph &known_graph,
                                           const std::vector<history::Constraint> &constraints) {
  // ? previous version: write to .agnf file
  // agnf_path = fs::current_path();
  // agnf_path.append("acyclicminisat_tmp_input.agnf");
  // try {
  //   utils::write_to_agnf_file(agnf_path, known_graph, constraints);
  // } catch (std::runtime_error &e) {
  //   // TODO   
  // }

  int n = known_graph.num_vertices();
  n_vertices = n;
  // 0. simplify dependency_graph and constraints
  auto simplify = [&](const history::DependencyGraph &known_graph, 
                      const std::vector<history::Constraint> &constraints
                      ) -> std::pair<SimplifiedKnownGraph, SimplifiedConstraints> {
    int cur_node_id = 0;
    auto node_id = std::unordered_map<int64_t, int>{};
    auto get_node_id = [&](int64_t x) -> int {
      if (node_id.contains(x)) return node_id.at(x);
      return node_id[x] = cur_node_id++;
    };
    auto simplified_known_graph = SimplifiedKnownGraph{};
    for (const auto &[from, to, _] : known_graph.edges()) {
      simplified_known_graph.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
    }
    auto simplified_constraints = SimplifiedConstraints{};
    for (const auto &constraint : constraints) {
      auto either_edges = SimplifiedEdgeSet{};
      for (const auto &[from, to, _] : constraint.either_edges) {
        either_edges.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
      }
      auto or_edges = SimplifiedEdgeSet{};
      for (const auto &[from, to, _] : constraint.or_edges) {
        or_edges.emplace_back(std::make_pair(get_node_id(from), get_node_id(to)));
      }
      simplified_constraints.emplace_back(std::make_pair(either_edges, or_edges));
    } 
    return std::make_pair(simplified_known_graph, simplified_constraints);
  };
  auto [simplified_known_graph, simplified_constraints] = simplify(known_graph, constraints);

  // std::cerr << simplified_known_graph.size() << std::endl;
  // for (const auto &[from, to] : simplified_known_graph) {
  //   std::cerr << from << " " << to << std::endl;
  // }
  // std::cerr << simplified_constraints.size() << std::endl;
  // for (const auto &[either_, or_] : simplified_constraints) {
  //   for (const auto &[from, to] : either_) std::cerr << from << " " << to << std::endl;
  //   for (const auto &[from, to] : or_) std::cerr << from << " " << to << std::endl;
  // }

  // 1. remove bridges and devide into components(by scc)
  auto auxgraph = utils::TarjanGraph(n);
  //  1.1 construct auxiliary graph 
  for (const auto &[from, to] : simplified_known_graph) {
    auxgraph.add_edge(from, to);
  }
  for (const auto &[either_, or_] : simplified_constraints) { // the word 'or' is reserved
    for (const auto &[from, to] : either_) {
      auxgraph.add_edge(from, to);
    }
    for (const auto &[from, to] : or_) {
      auxgraph.add_edge(from, to);
    }
  }
  //  1.2 remove bridges
  // auto no_bridge_known_graph = SimplifiedKnownGraph{};
  // auto no_bridge_constraints = SimplifiedConstraints{};
  
  int n_removed_bridges = 0;
  for (const auto &[from, to] : simplified_known_graph) {
    if (auxgraph.is_bridge(from, to)) {
      ++n_removed_bridges;
      continue;
    }
    no_bridge_known_graph.emplace_back(std::make_pair(from, to));
  }
  for (const auto &[either_, or_] : simplified_constraints) {
    auto new_either = SimplifiedEdgeSet{};
    for (const auto &[from, to] : either_) {
      if (auxgraph.is_bridge(from, to)) {
        ++n_removed_bridges;
        continue;
      } 
      new_either.emplace_back(std::make_pair(from, to));
    }
    auto new_or = SimplifiedEdgeSet{};
    for (const auto &[from, to] : or_) {
      if (auxgraph.is_bridge(from, to)) {
        ++n_removed_bridges;
        continue;
      } 
      new_or.emplace_back(std::make_pair(from, to));
    }
    no_bridge_constraints.emplace_back(std::make_pair(new_either, new_or));
  }

  // std::cerr << no_bridge_known_graph.size() << std::endl;
  // std::cerr << no_bridge_constraints.size() << std::endl;

  BOOST_LOG_TRIVIAL(debug) << "#removed bridges: " << n_removed_bridges;
  
  //  1.3 divide into components(by scc), not applied here
  // int n_components = auxgraph.get_n_scc();
  // polygraph_components.assign(n_components, std::pair<SimplifiedKnownGraph, SimplifiedConstraints>());
  // for (const auto &[from, to] : no_bridge_known_graph) {
  //   assert(auxgraph.get_scc(from) == auxgraph.get_scc(to));
  //   int bel_scc = auxgraph.get_scc(from);
  //   auto &[sub_known_gragh, _] = polygraph_components.at(bel_scc);
  //   sub_known_gragh.emplace_back(std::make_pair(from, to));
  // }
  // for (const auto &[either_, or_] : no_bridge_constraints) {
  //   int bel_scc = auxgraph.get_scc(either_.at(0).first);
  //   // checking consistency, can be skipped
  //   auto check = [&](const SimplifiedEdgeSet &edges) -> bool {
  //     for (const auto &[from, to] : edges) {
  //       if (auxgraph.get_scc(from) != bel_scc || auxgraph.get_scc(to) != bel_scc) {
  //         return false;
  //       }
  //     }
  //     return true;
  //   };
  //   assert(check(either_) && check(or_));
  //   auto &[_, sub_constraints] = polygraph_components.at(bel_scc);
  //   sub_constraints.emplace_back(std::make_pair(either_, or_));
  // }

} */

auto AcyclicMinisatSolver::solve() -> bool {
  bool ret = true;
  if (target_isolation_level == "ser") {
    ret = Minisat::am_solve(n_vertices, 
                            am_known_graph, 
                            am_constraints, 
                            n_sessions, 
                            n_total_transactions, 
                            n_total_events, 
                            write_steps, 
                            read_steps, 
                            txn_distance,
                            session_id_of);
  } else if (target_isolation_level == "si") {
    // TODO: heuristic pruning in SI
    ret = MinisatSI::am_solve(n_vertices, am_known_graph, am_constraints);
  }
  return ret;
}

AcyclicMinisatSolver::~AcyclicMinisatSolver() = default;

} // namespace checker::solver