#ifndef MINISAT_AGNF_H
#define MINISAT_AGNF_H

// #define USE_FSTREAM

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <utility>
#include <map>

#include "minisat/core/Polygraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/Graph.h"
#include "minisat/core/Solver.h"
#include "minisat/utils/ParseUtils.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/PairConflict.h"

namespace fs = std::filesystem;

namespace Minisat {

// constuct polygraph and make vars in sat solver
Polygraph *construct(const fs::path &agnf_path, Solver &solver) {
  return nullptr;
}

// Polygraph *construct(const fs::path &agnf_path, Solver &solver) {
//   // !FIXME: construct is now out of date
// #ifdef USE_FSTREAM
//   std::ifstream ifs;
//   ifs.open(agnf_path);
//   int n_vertices = 0, n_edges = 0, n_known_edges = 0, n_constraints = 0;
//   ifs >> n_vertices >> n_edges >> n_known_edges >> n_constraints;
//   Graph auxgraph(n_vertices); // auxiliary graph
//   Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
//   std::map<int, std::pair<int, int>> id_edge;
//   for (int x, y, i = 1; i <= n_edges; i++) {
//     ifs >> x >> y;
//     id_edge[i] = std::make_pair(x, y);
//     auxgraph.add_edge(x, y);
//   }
//   for (int x, y, i = 0; i < n_known_edges; i++) {
//     ifs >> x >> y;
//     if (auxgraph.is_bridge(x, y)) continue; // skip bridges, must not be in a cycle
//     polygraph->add_known_edge(x, y);
//   }
//   for (int i = 0; i < n_constraints * 2; i++) {
//     for (int id; ; ) {
//       ifs >> id;
//       if (id == 0) break;
//       auto &[from, to] = id_edge[id];
//       if (auxgraph.is_bridge(from, to)) continue;
//       polygraph->add_constraint_edge(i, from, to);
//     } 

//     if (i & 1) {
//       solver.newVar(), solver.newVar();
//       int v1 = i - 1, v2 = i;
//       vec<Lit> lits;
//       lits.push(mkLit(v1)), lits.push(mkLit(v2));
//       solver.addClause_(lits);
//       lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
//       solver.addClause_(lits);
//     }
//   }
//   ifs.close();
//   return polygraph;

// #else
//   FILE *inf = fopen(agnf_path.c_str(), "r");
//   int n_vertices = 0, n_edges = 0, n_known_edges = 0, n_constraints = 0;
//   // ifs >> n_vertices >> n_edges >> n_known_edges >> n_constraints;
//   fscanf(inf, "%d%d%d%d", &n_vertices, &n_edges, &n_known_edges, &n_constraints);
//   Graph auxgraph(n_vertices); // auxiliary graph
//   Polygraph *polygraph = new Polygraph(n_vertices, n_constraints * 2);
//   std::map<int, std::pair<int, int>> id_edge;
//   for (int x, y, i = 1; i <= n_edges; i++) {
//     // ifs >> x >> y;
//     fscanf(inf, "%d%d", &x, &y);
//     id_edge[i] = std::make_pair(x, y);
//     auxgraph.add_edge(x, y);
//   }
//   for (int x, y, i = 0; i < n_known_edges; i++) {
//     // ifs >> x >> y;
//     fscanf(inf, "%d%d", &x, &y);

// #ifdef SKIP_BRIDGE
//     if (auxgraph.is_bridge(x, y)) {
//   #ifdef MONITOR_ENABLED
//       Monitor::get_monitor()->skipped_bridge_count++;
//   #endif
//       continue; // skip bridges, must not be in a cycle
//     } 
// #endif

//     polygraph->add_known_edge(x, y);
//   }

//   // polygraph->rebuild_known_edges();

//   for (int i = 0; i < n_constraints * 2; i++) {
//     for (int id; ; ) {
//       // ifs >> id;
//       fscanf(inf, "%d", &id);
//       if (id == 0) break;
//       auto &[from, to] = id_edge[id];
      
// #ifdef SKIP_BRIDGE
//       if (auxgraph.is_bridge(from, to)) {
//     #ifdef MONITOR_ENABLED
//       Monitor::get_monitor()->skipped_bridge_count++;
//     #endif    
//         continue;
//       } 
// #endif

//       polygraph->add_constraint_edge(i, from, to);
//     } 

//     if (i & 1) {
//       solver.newVar(), solver.newVar();
//       int v1 = i - 1, v2 = i;
//       vec<Lit> lits;
//       lits.push(mkLit(v1)), lits.push(mkLit(v2));
//       solver.addClause_(lits);
//       lits.clear(), lits.push(~mkLit(v1)), lits.push(~mkLit(v2));
//       solver.addClause_(lits);
//     }
//   }
//   fclose(inf);

// #ifdef INIT_PAIR_CONFLICT
//   init_pair_conflict(polygraph, solver);
// #endif

//   return polygraph;
// #endif
// } 

}; // namespace Minisat

#endif // MINISAT_AGNF_H