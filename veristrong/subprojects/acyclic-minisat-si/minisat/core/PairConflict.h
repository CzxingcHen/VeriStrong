#ifndef MINISAT_SI_PAIRCONFLICT_H
#define MINISAT_SI_PAIRCONFLICT_H

#include <iostream>

#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Solver.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Logger.h"

namespace MinisatSI {

bool init_pair_conflict(AcyclicSolver &solver) {
  Logger::log("[Init Pair Conflict]");
  // Logger::log("SER cycle may not be a SI cycle, this part is left to be implemented.");
  // return false;

  Polygraph *polygraph = solver.get_polygraph();
  if (polygraph->n_vertices > 1000000) {
    Logger::log(fmt::format(" - failed!  polygraph has {} vertices, > limit 1m", polygraph->n_vertices));
    return false;
  }
  if (polygraph->n_vars > 10000000) {
    Logger::log(fmt::format(" - failed! solver has {} vars, > limit 10m", polygraph->n_vars));
    return false;
  }

  Graph graph = Graph(polygraph->n_vertices);
  auto dep_edges = std::vector<std::pair<int, int>>{};
  auto anti_dep_edges = std::vector<std::vector<int>>(polygraph->n_vertices, std::vector<int>{});

  for (const auto &[from, to, type] : polygraph->known_edges) {
    if (type != 3) { // dep edge, not RW
      graph.add_edge(from, to); // Graph helps handle duplicated edges
      dep_edges.emplace_back(from, to);
    } else { // anti dep edge, RW
      anti_dep_edges[from].emplace_back(to);
    }
    // std::cout << "x" << from << " " << to << std::endl;
  }
  for (const auto &[from, dep_to] : dep_edges) {
    for (const auto &anti_dep_to : anti_dep_edges[dep_to]) {
      graph.add_edge(from, anti_dep_to);
    }
  }

  auto conflict = [&polygraph, &graph](int v1, int v2) -> bool {
    auto &&edge = [&polygraph](int v) -> std::pair<int, int> {
      // * note: hard to extend to RW edges
      if (polygraph->is_ww_var(v)) {
        auto &[from, to, _] = polygraph->ww_info[v];
        return {from, to};
      } else if (polygraph->is_wr_var(v)) {
        auto &&[from, to, _] = polygraph->wr_info[v];
        return {from, to};
      }
      assert(false);
    };

    auto [from1, to1] = edge(v1);
    auto [from2, to2] = edge(v2);

    return graph.reachable(to1, from2) && graph.reachable(to2, from1); 
  };

  for (int v1 = 0; v1 < polygraph->n_vars; v1++) {
    for (int v2 = v1 + 1; v2 < polygraph->n_vars; v2++) {
      if (conflict(v1, v2)) {
        vec<Lit> lits;
        lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
        solver.addClause_(lits); 
        Logger::log(Logger::lits2str(lits));
      }
    }
  }

  return true;
}

// void init_pair_conflict(Polygraph *polygraph, Solver &solver) {
  // TODO: init pair conflict
  // ! deprecated
  // if (polygraph->n_vertices > 100000)  {
  //   std::cerr << "skip pair conflict generation: too many vertices" << std::endl;
  //   return; // skip this optimization when there are too many vertices
  // }
  // if (polygraph->edges.size() > 10000) {
  //   std::cerr << "skip pair conflict generation: too many constraints" << std::endl;
  //   return; // skip this optimization when there are too many constraints
  // } 
  // Graph graph = Graph(polygraph->n_vertices);
  // // add all edges of known graph
  // for (const auto &[from, to] : polygraph->known_edges) { graph.add_edge(from, to); }
  // auto conflict = [&](int v1, int v2) -> bool {
  //   if (polygraph->edges[v1].empty() || polygraph->edges[v2].empty()) return false;
  //   for (const auto &[from1, to1] : polygraph->edges[v1]) {
  //     for (const auto &[from2, to2] : polygraph->edges[v2]) {
  //       if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) {
  //         return true;
  //       }
  //       return false;
  //     }
  //   }
  //   return false;
  //   // const auto &[from1, to1] = polygraph->edges[v1][0];
  //   // const auto &[from2, to2] = polygraph->edges[v2][0];
  //   // if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) return true;
  //   // return false;
  // };
  // auto possible_triple_conflict = [&](int v1, int v2) -> bool {
  //   // TODO
  //   return false;
  //   if (polygraph->edges[v1].empty() || polygraph->edges[v2].empty()) return false;
  //   for (const auto &[from1, to1] : polygraph->edges[v1]) {
  //     for (const auto &[from2, to2] : polygraph->edges[v2]) {
  //       if (graph.reachable(to1, from2) && graph.reachable(to2, from1)) return false;
  //       if (graph.reachable(to1, from2) || graph.reachable(to2, from1)) return true;
  //       return false;
  //     }
  //   }
  //   return false;
  // };
  // // auto triple_conflict = // TODO
  // int n_vars = polygraph->edges.size();
  // for (int v1 = 0; v1 < n_vars; v1++) {
  //   for (int v2 = v1 + 1; v2 < n_vars; v2++) {
  //     if (conflict(v1, v2)) {
  //       vec<Lit> lits;
  //       lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
  //       solver.addClause_(lits);
  //     } else if (possible_triple_conflict(v1, v2)) {
  //       // TODO
  //     }
  //   }
  // }
// }

} // namespace MinisatSI

#endif // MINISAT_SI_PAIR_CONFLICT_H