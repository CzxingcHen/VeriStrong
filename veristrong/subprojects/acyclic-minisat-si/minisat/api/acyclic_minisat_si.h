#ifndef MINISAT_SI_ACYCLIC_MINISAT_H
#define MINISAT_SI_ACYCLIC_MINISAT_H

#include <utility>
#include <vector>

#include "minisat/api/acyclic_minisat_si_types.h"

namespace MinisatSI {

// precondition: known_graph and constraints must be remapped
bool am_solve(int n_vertices, const KnownGraph &known_graph, const Constraints &constraints);

} // namespace MinisatSI

#endif // MINISAT_SI_ACYCLIC_MINISAT_H