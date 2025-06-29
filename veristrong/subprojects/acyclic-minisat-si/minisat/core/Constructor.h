#ifndef MINISAT_SI_CONSTRCUTOR_H
#define MINISAT_SI_CONSTRCUTOR_H

#include <cassert>
#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Solver.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/PairConflict.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/Logger.h"

namespace MinisatSI {

Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, AcyclicSolver &solver, std::vector<Lit> &unit_lits) {
  Polygraph *polygraph = new Polygraph(n_vertices); // unused n_vars

  Logger::log("[Known Graph]");
  Logger::log(fmt::format("n = {}", n_vertices));
  for (const auto &[type, from, to, keys] : known_graph) {
    polygraph->add_known_edge(from, to, type, keys);
    Logger::log(fmt::format("{}: {} -> {}, keys = {}", Logger::type2str(type), from, to, Logger::vector2str(keys)));
  }

  Logger::log("[Constraints]");
  int var_count = 0;
  const auto &[ww_cons, wr_cons] = constraints;
  Logger::log("[1. WW Constraints]");
  for (const auto &[either_, or_, keys] : ww_cons) {
    solver.newVar(), solver.newVar();
    int v1 = var_count++, v2 = var_count++;

    auto keys_set = std::set(keys.begin(), keys.end());
    
    polygraph->map_ww_var(v1, either_, or_, keys_set);
    polygraph->map_ww_var(v2, or_, either_, keys_set);

    vec<Lit> lits; // v1 + v2 = 1 => (v1 | v2) & ((~v1) | (~v2))
    lits.push(mkLit(v1)), lits.push(mkLit(v2));
    solver.addClause_(lits); // (v1 | v2)
    Logger::log(Logger::lits2str(lits));
    lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
    solver.addClause_(lits); // ((~v1) | (~v2))
  }
  Logger::log("[2. WR Constraints]");
  assert(unit_lits.empty());
  for (const auto &[read, writes, key] : wr_cons) {
    vec<Lit> lits;
    auto cons = std::vector<int>{};
    for (const auto &write : writes) {
      solver.newVar();
      int v = var_count++; // WR(k)
      polygraph->map_wr_var(v, write, read, key);
      lits.push(mkLit(v));
      cons.emplace_back(v);
    }
    if (lits.size() != 1) {
      solver.addClause_(lits); // v1 | v2 | ... | vn
      int index = polygraph->add_wr_cons(cons);
      for (const auto &v : cons) {
        polygraph->map_wr_cons(v, index);
      }
    } else {
      unit_lits.emplace_back(lits[0]);
    }
    // TODO: in fact, v1 + v2 + ... + vn = 1,
    //       here we only consider the v1 + v2 + ... + vn >= 1 half,
    //       another part which may introduce great power of unit propagate is to be considered
    Logger::log(Logger::lits2str(lits));
  }

  polygraph->set_n_vars(var_count);
  std::cerr << "#vars: " << var_count << std::endl;

  Logger::log("[Var to Theory Interpretion]");
  for (int v = 0; v < var_count; v++) {
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      Logger::log(fmt::format("{}: WW, {} -> {}, keys = {}", v, from, to, Logger::set2str(keys)));
    } else if (polygraph->is_wr_var(v)) {
      const auto &[from, to, key] = polygraph->wr_info[v];
      Logger::log(fmt::format("{}: WR({}), {} -> {}", v, key, from, to));
    } else {
      assert(false);
    }
  }

  return polygraph;
}

// Polygraph *construct(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints, Solver &solver) {
//   int n_constraints = constraints.size();
//   Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
//   for (const auto &[from, to] : known_graph) {
//     polygraph->add_known_edge(from, to);
//   }
//   int var_count = 0;
//   for (const auto &[either_, or_] : constraints) {
//     auto add_constraint = [&](const EdgeSet &edges) -> void {
//       for (const auto &[from, to] : edges) {
//         polygraph->add_constraint_edge(var_count, from, to);
//       }
//       ++var_count;
//     };
//     add_constraint(either_), add_constraint(or_);
//   }
//   assert(var_count == n_constraints * 2);
//   for (int i = 0; i < var_count; i += 2) {
//     solver.newVar(), solver.newVar();
//     int v1 = i, v2 = i + 1;
//     vec<Lit> lits;
//     lits.push(mkLit(v1)), lits.push(mkLit(v2));
//     solver.addClause_(lits);
//     lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
//     solver.addClause_(lits);
//   }

// #ifdef INIT_PAIR_CONFLICT
//   init_pair_conflict(polygraph, solver);
// #endif

//   return polygraph;
// }

} // namespace MinisatSI

#endif // MINISAT_SI_CONSTRCUTOR_H