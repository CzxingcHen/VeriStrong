#ifndef CHECKER_SOLVER_FACTORY_H
#define CHECKER_SOLVER_FACTORY_H

#include <string>

#include "history/constraint.h"
#include "history/dependencygraph.h"

#include "solver/abstractSolver.h"
#include "solver/z3Solver.h"
#include "solver/monosatSolver.h"
#include "solver/acyclicMinisatSolver.h"
// TODO: implement more solvers

namespace checker::solver {
  struct SolverFactory {
private:    
    static const SolverFactory solverFactory;

public:
    auto make(const std::string &solver_type, 
              const history::DependencyGraph &dependency_graph,
              const history::Constraints &constraints,
              const history::HistoryMetaInfo &history_meta_info,
              const std::string &isolation_level) -> AbstractSolver * {
                if (solver_type == "z3") {
                  return new Z3Solver{dependency_graph, constraints, history_meta_info, isolation_level};
                } else if (solver_type == "monosat") {
                  return new MonosatSolver{dependency_graph, constraints, history_meta_info, isolation_level, /* enable_unique_encoding = */ false};
                } else if (solver_type == "monosat-baseline") {
                  return new MonosatSolver{dependency_graph, constraints, history_meta_info, isolation_level, /* enable_unique_encoding = */ true};
                } else if (solver_type == "acyclic-minisat") {
                  return new AcyclicMinisatSolver{dependency_graph, constraints, history_meta_info, isolation_level};
                } else {
                  throw std::runtime_error("unknown solver!");
                }
              }

    static auto getSolverFactory() -> SolverFactory { return solverFactory; }
  };
}

#endif
