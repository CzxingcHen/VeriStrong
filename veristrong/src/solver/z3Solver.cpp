#include "z3Solver.h"

// #include <z3++.h>

#include <algorithm>
#include <boost/container_hash/extensions.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/ranges.h"
#include "utils/to_container.h"
#include "utils/toposort.h"

using boost::add_edge;
using boost::add_vertex;
using boost::adjacency_list;
using boost::bidirectionalS;
using boost::directedS;
using boost::edge;
using boost::hash_setS;
using boost::no_property;
using boost::num_vertices;
using boost::remove_edge;
using boost::source;
using boost::target;
using boost::vecS;
using boost::vertex_index_t;
using checker::history::Constraints;
using checker::utils::to;
using std::back_inserter;
using std::get;
using std::inserter;
using std::nullopt;
using std::optional;
using std::pair;
using std::reduce;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::copy;
using std::ranges::range;
using std::ranges::subrange;
using std::ranges::views::all;
using std::ranges::views::drop;
using std::ranges::views::filter;
using std::ranges::views::iota;
using std::ranges::views::keys;
using std::ranges::views::reverse;
using std::ranges::views::single;
using std::ranges::views::transform;
// using z3::expr;
// using z3::expr_vector;

// template <>
// struct std::hash<expr> {
//   auto operator()(const expr &e) const -> size_t { return e.hash(); }
// };

template <>
struct std::hash<pair<adjacency_list<>::vertex_descriptor,
                      adjacency_list<>::vertex_descriptor>> {
  auto operator()(const pair<adjacency_list<>::vertex_descriptor,
                             adjacency_list<>::vertex_descriptor> &p) const
      -> size_t {
    auto h = boost::hash<adjacency_list<>::vertex_descriptor>{};
    return h(p.first) ^ h(p.second);
  }
};

// template <>
// struct std::equal_to<expr> {
//   auto operator()(const expr &left, const expr &right) const -> bool {
//     return left.id() == right.id();
//   }
// };

namespace checker::solver {

Z3Solver::Z3Solver(const history::DependencyGraph &known_graph,
                   const history::Constraints &constraints,
                   const history::HistoryMetaInfo &history_meta_info,
                   const std::string &isolation_level) {
  std::cerr << "Not implemented!" << std::endl;
  assert(0);
}

// Z3Solver::Z3Solver(const history::DependencyGraph &known_graph,
//                const vector<history::Constraint> &constraints) 
//     : solver{context, z3::solver::simple{}} {
//   using Graph = adjacency_list<hash_setS, vecS, directedS, int64_t>;
//   using Vertex = Graph::vertex_descriptor;
//   using Edge = Graph::edge_descriptor;
//   using EdgeSet = unordered_set<Edge, boost::hash<Edge>>;

//   CHECKER_LOG_COND(trace, logger) {
//     logger << "wr:\n" << known_graph.wr << "cons:\n";
//     for (const auto &c : constraints) {
//       logger << c;
//     }
//   }

//   auto polygraph = Graph{};
//   auto vertex_map = unordered_map<int64_t, Vertex>{};
//   auto edge_map = unordered_map<pair<Vertex, Vertex>, Edge>{};

//   auto get_vertex = [&](int64_t id) {
//     if (vertex_map.contains(id)) {
//       return vertex_map.at(id);
//     }
//     return vertex_map.try_emplace(id, add_vertex(id, polygraph)).first->second;
//   };
//   auto get_edge = [&](int64_t from, int64_t to) {
//     auto from_desc = get_vertex(from);
//     auto to_desc = get_vertex(to);

//     if (auto it = edge_map.find({from_desc, to_desc}); it != edge_map.end()) {
//       return it->second;
//     } else {
//       return edge_map
//           .try_emplace({from_desc, to_desc},
//                        add_edge(from_desc, to_desc, polygraph).first)
//           .first->second;
//     }
//   };
//   auto add_constraint_edge = transform([&](const Constraint::Edge &e) {
//     const auto &[from, to, _] = e;
//     return get_edge(from, to);
//   });

//   auto constraint_edges = unordered_map<expr, vector<Edge>>{};

//   // use true as a dummy constraint variable for known edges
//   auto z3_true = context.bool_val(true);
//   auto known_edges = EdgeSet{};
//   for (const auto &[from, to, _] : known_graph.edges()) {
//     BOOST_LOG_TRIVIAL(trace) << "known: " << from << "->" << to;
//     known_edges.emplace(get_edge(from, to));
//   }
//   constraint_edges.try_emplace(z3_true, known_edges | to<vector<Edge>>);

//   for (const auto &c : constraints) {
//     std::stringstream either_name, or_name;
//     either_name << c.either_txn_id << "->" << c.or_txn_id;
//     or_name << c.or_txn_id << "->" << c.either_txn_id;

//     auto either_var = context.bool_const(either_name.str().c_str());
//     auto or_var = context.bool_const(or_name.str().c_str());
//     solver.add(either_var ^ or_var);

//     constraint_edges.try_emplace(
//         either_var, c.either_edges | add_constraint_edge | to<vector<Edge>>);
//     constraint_edges.try_emplace(
//         or_var, c.or_edges | add_constraint_edge | to<vector<Edge>>);
//   }

//   CHECKER_LOG_COND(trace, logger) {
//     logger << "vertex_map:";
//     for (auto [a, b] : vertex_map) {
//       logger << ' ' << a << ":" << b;
//     }
//   }

//   CHECKER_LOG_COND(trace, logger) {
//     logger << "constraint_edges:";
//     for (auto &&[c, v] : constraint_edges) {
//       logger << c.to_string() << '[';
//       for (auto i = 0_uz; i < v.size(); i++) {
//         logger << v[i];
//         if (i != v.size() - 1) {
//           logger << ' ';
//         }
//       }
//       logger << "] ";
//     }
//   }

//   user_propagator = std::make_unique<DependencyGraphHasNoCycle>(
//       solver, std::move(polygraph), std::move(constraint_edges));
// }

auto Z3Solver::solve() -> bool { return false; }

Z3Solver::~Z3Solver() = default;

// struct DependencyGraphHasNoCycle : z3::user_propagator_base {
//   using PolyGraph = adjacency_list<hash_setS, vecS, directedS, int64_t>;
//   using PolyGraphVertex = PolyGraph::vertex_descriptor;
//   using PolyGraphEdge = PolyGraph::edge_descriptor;
//   using PolyGraphEdgeSet =
//       unordered_set<PolyGraphEdge, boost::hash<PolyGraphEdge>>;

//   using Graph =
//       adjacency_list<vecS, vecS, directedS, no_property, optional<expr>>;
//   using Vertex = Graph::vertex_descriptor;
//   using Edge = Graph::edge_descriptor;

//   static_assert(std::is_same_v<PolyGraphVertex, Vertex> &&
//                 std::is_integral_v<Vertex>);

//   PolyGraph polygraph;

//   /*
//    * A dependency graph contains the current set of fixed edges of the
//    * polygraph. Each edge has a set of variables that enables it. These
//    * variables are currently assigned true by Z3.
//    */
//   Graph dependency_graph;
//   utils::IncrementalCycleDetector<Graph> cycle_detector;
//   bool has_conflict = false;

//   /*
//    * Z3 uses a stack of fixed variables internally. When push() is called, a new
//    * scope is pushed to the stack. When pop(lvl) is called, $lvl scopes are
//    * poped from the stack. Each scope contains one or more fixed variables.
//    * fixed(var, val) is called when a variable is assigned a value.
//    *
//    * We maintain a stack that mirrors the structure of Z3's using the variables
//    * fixed_vars_num and fixed_vars.
//    */
//   vector<size_t> fixed_vars_num;   // total number of fixed variables at this
//                                    // scope and lower scope
//   vector<expr> fixed_vars;         // fixed variables at each scope
//   vector<size_t> fixed_edges_num;  // total number of fixed edges
//   vector<Edge> fixed_edges;        // stack of fixed edges

//   // SMT variable -> polygraph edges to add
//   unordered_map<expr, vector<PolyGraphEdge>> constraint_to_edge_map;

//   // SMT variable -> propagate expr, used for WW propagation
//   unordered_map<expr, expr> propagate_map;

//   // edge -> conflicting SMT variables, used for incremental pruning
//   unordered_map<PolyGraphEdge, vector<expr>, boost::hash<PolyGraphEdge>>
//       conflict_map;

//   // statistics
//   size_t n_fixed_called = 0;
//   size_t n_conflicts_returned = 0;
//   size_t n_conflict_vars_returned = 0;
//   size_t n_pop_called = 0;
//   size_t n_pop_scopes = 0;
//   size_t n_propagate_called = 0;
//   size_t n_propagate_vars = 0;

//   DependencyGraphHasNoCycle(
//       z3::solver &solver, PolyGraph &&polygraph,
//       unordered_map<expr, vector<Edge>> &&constraint_edges)
//       : z3::user_propagator_base{&solver},
//         polygraph{std::move(polygraph)},
//         dependency_graph{num_vertices(this->polygraph)},
//         cycle_detector{dependency_graph},
//         constraint_to_edge_map{std::move(constraint_edges)},
//         propagate_map{[&]() {
//           auto ww_edge_to_var =
//               constraint_to_edge_map                                 //
//               | filter([](auto &&p) { return !p.first.is_true(); })  //
//               | transform([](auto &&p) {
//                   return pair{p.second.front(), p.first};
//                 })  //
//               | to<unordered_map<PolyGraphEdge, expr,
//                                  boost::hash<PolyGraphEdge>>>;

//           CHECKER_LOG_COND(trace, logger) {
//             logger << "ww_edge_to_var:";
//             for (auto &&[e, p] : ww_edge_to_var) {
//               logger << ' ' << boost::source(e, this->polygraph) << "->"
//                      << boost::target(e, this->polygraph) << "=>"
//                      << p.to_string();
//             }
//           }

//           return constraint_to_edge_map  //
//                  | transform([&](auto &&p) {
//                      auto vars =
//                          p.second  //
//                          | filter([&](auto &&e) {
//                              return ww_edge_to_var.contains(e) &&
//                                     ww_edge_to_var.at(e).id() != p.first.id();
//                            })  //
//                          | transform([&](auto &&e) {
//                              return ww_edge_to_var.at(e);
//                            })  //
//                          | to<expr_vector>(ctx());
//                      return pair{
//                          p.first,
//                          z3::mk_and(vars),
//                      };
//                    })  //
//                  |
//                  filter([&](auto &&p) { return p.second.num_args() != 0; })  //
//                  | to<unordered_map<expr, expr>>;
//         }()},
//         conflict_map{[&]() {
//           auto m = unordered_map<PolyGraphEdge, vector<expr>,
//                                  boost::hash<PolyGraphEdge>>{};

//           for (auto &&[ex, edges] : this->constraint_to_edge_map) {
//             for (auto &&e : edges) {
//               m[e].emplace_back(ex);
//             }
//           }

//           return m;
//         }()} {
//     for (const auto &var : keys(constraint_to_edge_map)) {
//       BOOST_LOG_TRIVIAL(trace) << "add: " << var.to_string();
//       add(var);
//     }

//     CHECKER_LOG_COND(trace, logger) {
//       logger << "propagate_map:\n";
//       for (auto &&[v, p] : propagate_map) {
//         logger << v.to_string() << "=>";
//         for (auto &&v2 : p) {
//           logger << ' ' << v2.to_string();
//         }
//         logger << '\n';
//       }
//     }

//     register_fixed();
//   }

//   auto push() -> void override {
//     BOOST_LOG_TRIVIAL(trace) << "push";
//     assert(!has_conflict);
//     fixed_vars_num.emplace_back(fixed_vars.size());
//     fixed_edges_num.emplace_back(fixed_edges.size());
//   }

//   auto pop(unsigned int num_scopes) -> void override {
//     n_pop_called++;
//     n_pop_scopes += num_scopes;
//     BOOST_LOG_TRIVIAL(trace) << "pop " << num_scopes;

//     auto remaining_scopes = fixed_vars_num.size() - num_scopes;
//     auto remaining_vars_num = fixed_vars_num.at(remaining_scopes);
//     auto remaining_edges_num = fixed_edges_num.at(remaining_scopes);
//     fixed_vars_num.resize(remaining_scopes);
//     fixed_edges_num.resize(remaining_scopes);

//     CHECKER_LOG_COND(trace, logger) {
//       logger << "unfix:";
//       for (auto &&var : fixed_vars | drop(remaining_vars_num)) {
//         logger << ' ' << var.to_string();
//       }
//     }

//     for (auto &&e : fixed_edges | drop(remaining_edges_num)) {
//       boost::remove_edge(e, dependency_graph);
//     }

//     has_conflict = false;
//     fixed_vars.erase(fixed_vars.begin() + remaining_vars_num, fixed_vars.end());
//     fixed_edges.erase(fixed_edges.begin() + remaining_edges_num,
//                       fixed_edges.end());
//   }

//   auto fixed(const expr &var, const expr &value) -> void override {
//     if (has_conflict || value.is_false()) {
//       return;
//     }

//     BOOST_LOG_TRIVIAL(trace) << "fixed: " << var.to_string();
//     n_fixed_called++;
//     fixed_vars.push_back(var);

//     for (const auto &pe : constraint_to_edge_map.at(var)) {
//       auto e = add_edge(source(pe, polygraph), target(pe, polygraph), var,
//                         dependency_graph)
//                    .first;
//       fixed_edges.emplace_back(e);

//       if (!detect_cycle(e)) {
//         has_conflict = true;
//         return;
//       }

//       // propagate_edge(var, e);
//     }

//     propagate_var(var);
//   }

//   auto fresh(z3::context &ctx) -> z3::user_propagator_base * override {
//     return this;
//   }

//   // incremental cycle detection
//   auto detect_cycle(Edge added_edge) -> bool {
//     auto cycle = cycle_detector.check_add_edge(added_edge);
//     if (!cycle) {
//       return true;
//     }

//     CHECKER_LOG_COND(trace, logger) {
//       logger << "conflict:";
//       for (auto e : cycle.value()) {
//         logger << ' ' << polygraph[source(e, dependency_graph)] << "->"
//                << polygraph[target(e, dependency_graph)];
//       }
//     }

//     auto cycle_exprs = z3::expr_vector{ctx()};
//     for (auto e : cycle.value()) {
//       cycle_exprs.push_back(dependency_graph[e].value());
//     }

//     CHECKER_LOG_COND(trace, logger) {
//       logger << "conflict expr:";
//       for (const auto &e : cycle_exprs) {
//         logger << ' ' << e.to_string();
//       }
//     }

//     n_conflicts_returned++;
//     n_conflict_vars_returned += cycle_exprs.size();
//     conflict(cycle_exprs);
//     return false;
//   }

//   // assignment propagation using WW edges
//   auto propagate_var(const expr &var) -> void {
//     auto it = propagate_map.find(var);
//     if (it == propagate_map.end()) {
//       return;
//     }

//     n_propagate_called++;
//     n_propagate_vars += it->second.num_args();
//     CHECKER_LOG_COND(trace, logger) {
//       logger << "propagate: " << var.to_string() << "=>";
//       for (auto &&v : it->second.args()) {
//         logger << ' ' << v.to_string();
//       }
//     }

//     propagate(single(var) | to<expr_vector>(ctx()), it->second);
//   }

//   auto propagate_edge(const expr &var, Edge e) -> void {
//     auto from = source(e, dependency_graph);
//     auto to = target(e, dependency_graph);

//     auto conseq = ctx().bool_val(true);
//     auto successors = std::unordered_set<Vertex>{};
//     auto edges = std::unordered_set<PolyGraphEdge, boost::hash<PolyGraphEdge>>{};

//     std::function<void(Vertex)> get_successors = [&](Vertex current){
//       for (auto e : utils::as_range(boost::out_edges(current, dependency_graph))) {
//         auto to = target(e, dependency_graph);
//         edges.emplace(edge(current, to, polygraph).first);

//         if (successors.contains(to)) {
//           continue;
//         }

//         successors.emplace(to);
//         get_successors(to);
//       }
//     };

//     get_successors(to);

//     for (auto e : edges) {
//       if (auto it = conflict_map.find(e); it != conflict_map.end()) {
//         for (auto &&v : it->second) {
//           conseq = conseq && !v;
//         }
//       }
//     }

//     // propagate(single(var) | utils::to<expr_vector>(ctx()), conseq);
//   }

//   ~DependencyGraphHasNoCycle() override {
//     BOOST_LOG_TRIVIAL(debug) << "fixed() called " << n_fixed_called << " times";
//     BOOST_LOG_TRIVIAL(debug)
//         << "found conflict " << n_conflicts_returned << " times";
//     BOOST_LOG_TRIVIAL(debug)
//         << "avg. conflict length: "
//         << static_cast<float>(n_conflict_vars_returned) / n_conflicts_returned;
//     BOOST_LOG_TRIVIAL(debug) << "pop() called " << n_pop_called << " times";
//     BOOST_LOG_TRIVIAL(debug) << "avg. pop scopes: "
//                              << static_cast<float>(n_pop_scopes) / n_pop_called;
//     BOOST_LOG_TRIVIAL(debug)
//         << "propagate() called " << n_propagate_called << " times";
//     BOOST_LOG_TRIVIAL(debug)
//         << "avg. propagate var: "
//         << static_cast<float>(n_propagate_vars) / n_propagate_called;
//   }
// };

}  // namespace checker::solver
