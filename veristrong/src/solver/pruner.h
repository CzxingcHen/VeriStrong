#ifndef CHECKER_SOLVER_PRUNER_H
#define CHECKER_SOLVER_PRUNER_H

#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace checker::solver {
auto prune_unit_constraints(history::DependencyGraph &dependency_graph,
                            history::Constraints &constraints) -> bool;

auto prune_basic_constraints(history::DependencyGraph &dependency_graph,
                             history::Constraints &constraints) -> bool;                            

// TODO: SI version of prune_unit_constraints

auto prune_constraints(history::DependencyGraph &dependency_graph,
                       history::Constraints &constraints) -> bool;

auto fast_prune_constraints(history::DependencyGraph &dependency_graph,
                       history::Constraints &constraints) -> bool;

auto prune_si_constraints(history::DependencyGraph &dependency_graph,
                       history::Constraints &constraints) -> bool;

auto fast_prune_si_constraints(history::DependencyGraph &dependency_graph,
                       history::Constraints &constraints) -> bool;



}

#endif  // CHECKER_SOLVER_PRUNER_H
