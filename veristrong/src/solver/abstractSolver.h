#ifndef CHECKER_SOLVER_ABSTRACT_SOLVER_H
#define CHECKER_SOLVER_ABSTRACT_SOLVER_H

#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {

struct AbstractSolver {
  AbstractSolver() {}
  virtual auto solve() -> bool { return true; };
  virtual ~AbstractSolver() {};
};
} // namespace

#endif // CHECKER_SOLVER_ABSTRACT_SOLVER_H