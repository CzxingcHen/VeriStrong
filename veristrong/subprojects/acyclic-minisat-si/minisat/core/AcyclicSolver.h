#ifndef MINISAT_SI_ACYCLICSOLVER_H
#define MINISAT_SI_ACYCLICSOLVER_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "minisat/core/Solver.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/core/AcyclicSolverHelper.h"
#include "minisat/mtl/Vec.h"

namespace MinisatSI {
class AcyclicSolver : public Solver {
  vec<int> atom_trail;
  vec<int> atom_trail_lim;
  vec<Lit> propagated_lits_trail;
  vec<int> propagated_lits_trail_lim;
  std::unordered_map<int, bool> added_var;
  std::unordered_map<int, std::unordered_set<int>> conflict_vars_of;

  std::unordered_map<int, bool> cancelled;

  AcyclicSolverHelper *solver_helper;

protected:
  void add_atom(int var);
  void newDecisionLevel(); // override
  CRef propagate(); // override
  void cancelUntil(int level); // override
  Lit pickBranchLit(); // override

  lbool search(int nof_conflicts); // same as Solver.h

public:
  AcyclicSolver();
  void init(AcyclicSolverHelper *_solver_helper);
  bool addClause_(vec<Lit>& ps);  // same as Solver.h, but will not call proper propagate()
  bool simplify(); // same as Solver.h, but will not call proper propagate()
  lbool solve_(); // same as Solver.h
  bool solve();
  ~AcyclicSolver();

  // getter
  AcyclicSolverHelper *get_solver_helper();
  Polygraph *get_polygraph();
};
} // namespace MinisatSI

#endif // MINISAT_SI_ACYCLICSOLVER_H