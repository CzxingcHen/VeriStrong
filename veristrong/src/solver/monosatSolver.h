#ifndef CHECKER_SOLVER_MONOSATSOLVER_H
#define CHECKER_SOLVER_MONOSATSOLVER_H

#include <Monosat.h>

#include <vector>
#include <string>
#include <filesystem>

#include "abstractSolver.h"

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::solver {
struct MonosatSolver : AbstractSolver {
  SolverPtr solver;
  fs::path gnf_path;
  bool enable_unique_encoding = false;

  MonosatSolver(const history::DependencyGraph &known_graph,
                const history::Constraints &constraints,
                const history::HistoryMetaInfo &history_meta_info,
                const std::string &isolation_level,
                const bool _enable_unique_encoding);

  auto solve() -> bool override;

  ~MonosatSolver();
};
} // namespace

#endif // CHECKER_SOLVER_MONOSATSOLVER_H