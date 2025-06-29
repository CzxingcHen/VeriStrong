#include <fmt/format.h>

#include "minisat/api/acyclic_minisat_si.h"
#include "minisat/core/Solver.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/core/Constructor.h"
#include "minisat/core/Logger.h"

namespace MinisatSI {

bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints) {
  Logger::log("[Acyclic Minisat QxQ]");

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
  Polygraph *polygraph = construct(n_vertices, known_graph, constraints, S, unit_lits);
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

  if (!S.simplify()) {
    Logger::log("[Conflict detected in simplify()!]");
    Logger::log(fmt::format("[Accept = {}]", false));
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
  return accept;
}

} // namespace MinisatSI