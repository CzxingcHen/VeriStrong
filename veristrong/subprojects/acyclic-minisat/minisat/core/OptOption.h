#ifndef MINISAT_OPT_OPTION_H
#define MINISAT_OPT_OPTION_H

// optimization options switch here

// #define VISUALIZE_UNSAT_CONFLICT 1

// #define CHECK_ACYCLICITY 1

// #define SKIP_BRIDGE 1

// #define ENCODE_WR_UNIQUE 1

// #define OUTER_RW_DERIVATION 1

// #define ENDENSER_KNOWN_GRAPH 1
// #define ENDENSER_RATIO 0.3

// #define REDUCE_KNOWN_GRAPH 1     

// #define FIND_MINIMAL_CYCLE 1

// #define HEURISTIC_TOPO_ORDER 1

// * note: INIT_BY_SESS would be overwritten by INIT_BY_RANDOM
// #define HEURISTIC_TOPO_INIT_BY_SESS 1

// #define HEURISTIC_TOPO_INIT_RANDOM 1

// #define HEURISTIC_DIST_INIT_TOPO 1

// use ICD algorithm default
#define PK_TOPO_ALGORITHM 1

#define INIT_PAIR_CONFLICT 1

// #define ENCODE_MORE_CONFLICT 1

#define CONFLICT_WIDTH 3

#define MONITOR_ENABLED 1

// #define LOGGER_ENABLED 1

// WR constraint propagation
#define ENABLE_WRCP 1 

// #define ENABLE_UEP 1
// !NOT Implemented

// #define EXTEND_VERTICES_IN_UEP 1

// #define EXTEND_KG_IN_UEP 1

// #define SELECT_MAX_EDGES 1
// #define SELECT_MIN_EDGES 1

#define ADDED_EDGES_CACHE 1

#define INDUCE_KNOWN_EDGES 1

#endif