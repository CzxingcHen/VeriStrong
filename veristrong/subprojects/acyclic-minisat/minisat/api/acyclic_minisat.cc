#include <algorithm>

#include <fmt/format.h>

#include "minisat/api/acyclic_minisat.h"
#include "minisat/core/Solver.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/core/Constructor.h"
#include "minisat/core/Logger.h"

namespace Minisat {

bool am_solve_with_suggestion(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints,
                              int suggest_distance,
                              // history meta info
                              const std::unordered_map<int, std::unordered_map<int64_t, int>> &write_steps,
                              const std::unordered_map<int, std::unordered_map<int64_t, int>> &read_steps,
                              const std::unordered_map<int, int> &txn_distance,
                              const int n_sessions, const int n_total_transactions,
                              const std::unordered_map<int, int> &session_id_of) {
  // Logger::log(fmt::format("[Acyclic Minisat QxQ starts a new solving pass with suggest_distance = {}]", suggest_distance));
  auto start_time = std::chrono::high_resolution_clock::now();

  AcyclicSolver S;
  auto show_model = [&S](std::string model_name = "Model") -> void {
    Logger::log(fmt::format("[{}]", model_name));
    bool all_undef = true;
    for (int i = 0; i < S.nVars(); i++) {
      if (S.value(i) != l_Undef) {
        Logger::log(fmt::format("- {} = {}", i, (S.value(i) == l_True)));
        all_undef = false;
      }
    }
    if (all_undef) Logger::log("- all vars remain UNDEF");
  };

  auto unit_lits = std::vector<Lit>{};
  // This is a BAD Implementation, for we have to resolve the cycle dependency conflict of adding unit clauses into theory solver and initialization of SAT solver
  Polygraph *polygraph = construct(n_vertices, known_graph, constraints, S, unit_lits, suggest_distance, write_steps, read_steps, txn_distance, n_sessions, n_total_transactions, session_id_of);
  if (polygraph->n_vars == 0) {
    auto end_time = std::chrono::high_resolution_clock::now();
    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->encode_time += 
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    #endif

    #ifdef MONITOR_ENABLED
      Monitor::get_monitor()->show_statistics();
    #endif
    return true; // if pruned, this is actually true
  } 

  assert(polygraph->construct_known_graph_reachablity());

  AcyclicSolverHelper *solver_helper = nullptr;
  try {
    solver_helper = new AcyclicSolverHelper(polygraph);
  } catch (std::runtime_error &e) {
    return false; // UNSAT
  }
  S.init(solver_helper);

  // ------------
  // This part of code actually is a responsibility of Constructor
  for (const Lit &l : unit_lits) {
    vec<Lit> lits; lits.push(l);
    S.addClause_(lits);
  }
  // ------------

#ifdef INIT_PAIR_CONFLICT
  init_pair_conflict(S);
#endif

  auto end_time = std::chrono::high_resolution_clock::now();
  #ifdef MONITOR_ENABLED
    Monitor::get_monitor()->encode_time += 
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  #endif

  if (!S.simplify()) {
    Logger::log("[Conflict detected in simplify()!]");
    Logger::log(fmt::format("[Accept = {}]", false));

#ifdef VISUALIZE_UNSAT_CONFLICT
    assert(!S.final_conflict.empty());
    for (const int lit : S.final_conflict) {
      std::cout << lit << ", ";
    }
    std::cout << std::endl;
#endif

    return false; // UNSAT, decided by unit propagation
  } 
  show_model("Model after simplify()");
  Logger::log("[Search Traits]");
  bool accept = S.solve();
  delete polygraph;

#ifdef MONITOR_ENABLED
  Monitor::get_monitor()->show_statistics();
#endif

  Logger::log(fmt::format("[Accept = {}]", accept));
  if (accept) {
    Logger::log("[Model Founded]");
    for (int i = 0; i < S.nVars(); i++) {
      if (S.model[i] != l_Undef) {
        Logger::log(fmt::format("- {} = {}", i, (S.model[i] == l_True)));
      } else {
        Logger::log(fmt::format("- {} remains UNDEF", i));
      }
    }
  }

#ifdef VISUALIZE_UNSAT_CONFLICT
  if (!accept) {
    assert(!S.final_conflict.empty());
    for (const int lit : S.final_conflict) {
      std::cout << lit << ", ";
    }
    std::cout << std::endl;
  }
#endif

  return accept;
}

bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints,
              // history meta info
              const int n_sessions, const int n_total_transactions, const int n_total_events,
              const std::unordered_map<int, std::unordered_map<int64_t, int>> &write_steps,
              const std::unordered_map<int, std::unordered_map<int64_t, int>> &read_steps,
              const std::unordered_map<int, int> &txn_distance,
              const std::unordered_map<int, int> &session_id_of) {
  Logger::log("[Acyclic Minisat QxQ]");

  bool accept = false;
  int test_round = 0;
  // for (int dist = std::max(1, n_total_events / n_total_transactions * 6); dist <= n_total_events; dist <<= 1) { // TODO: incresing strategy
  //   Logger::log(fmt::format("suggest dist = {}", dist));
  //   std::cout << "suggest dist = " << dist << std::endl;
  //   accept = am_solve_with_suggestion(n_vertices, known_graph, constraints, dist, write_steps, read_steps);
  //   ++test_round;
  //   if (accept) break;
  // }
  if (!accept) {
    // std::cout << "suggest dist = -1\n";
    accept = am_solve_with_suggestion(n_vertices, known_graph, constraints, /* dist = */ -1, write_steps, read_steps, txn_distance, n_sessions, n_total_transactions, session_id_of);
    ++test_round;
  } 

  // std::cout << "Solve Round = " << test_round << std::endl;
  // Logger::log(fmt::format("[Solve Round = {}]", test_round));
  Logger::log(fmt::format("[Eventual Accept = {}]", accept));

  return accept;
}

} // namespace Minisat