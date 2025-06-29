// FIXME: see google cpp standard to reorder head files

#include "AcyclicSolverHelper.h" 

#include <iostream>
#include <vector>
#include <bitset>
#include <tuple>
#include <stack>
#include <cassert>
#include <fmt/format.h>

#include "minisat/core/Polygraph.h"
#include "minisat/core/ICDGraph.h"
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Vec.h"
#include "minisat/core/OptOption.h"
#include "minisat/core/Logger.h"

namespace MinisatSI {

AcyclicSolverHelper::AcyclicSolverHelper(Polygraph *_polygraph) {
  polygraph = _polygraph;
  icd_graph.init(polygraph->n_vertices, polygraph->n_vars, polygraph);
  conflict_clauses.clear();
  ww_to.assign(polygraph->n_vertices, {});
  wr_to.assign(polygraph->n_vertices, {});
  added_edges_of.assign(polygraph->n_vars, {});
  known_induced_edges_of.assign(polygraph->n_vars, {});
  
  // C++ is really interesting...
  dep_edges_of.assign(polygraph->n_vertices, {});
  anti_dep_edges_of.assign(polygraph->n_vertices, {});
  // The above code cannot work, but the beneath code works.
  // for (int i = 0; i < polygraph->n_vertices; i++) {
  //   dep_edges_of.emplace_back(std::multiset<std::pair<int, int>, decltype(pair_hash_endpoint)>{});
  //   anti_dep_edges_of.emplace_back(std::multiset<std::tuple<int, int, int>, decltype(triple_hash_endpoint2)>{});
  // }
  
  for (const auto &[from, to, type] : polygraph->known_edges) {
    if (type != 3) { // SO or WW or WR
      add_induced_dep_edge(/* var = */ -1, from, to, /* dep_reason = */ -1);
    } else {
      add_induced_anti_dep_edge(/* var = */ -1, from, to, /* ww_reason = */ -1, /* wr_reason = */ -1);
    }
    if (type == 1) { // WW
      assert(polygraph->has_ww_keys(from, to));
      const auto &keys = polygraph->ww_keys[from][to];
      ww_keys[from][to].insert(keys.begin(), keys.end());
      for (const auto &key : keys) {
        ww_to[from][key].insert(to);
      } 
    } else if (type == 2) { // WR
      assert(polygraph->has_wr_keys(from, to));
      for (const auto &key : polygraph->wr_keys[from][to]) {
        wr_to[from][key].insert(to);
      }
    }
  }
  for (int v = 0; v < polygraph->n_vars; v++) { // move ww keys (in constraints) into a unified position
    if (polygraph->is_ww_var(v)) {
      const auto &[from, to, keys] = polygraph->ww_info[v];
      ww_keys[from][to].insert(keys.begin(), keys.end());
    }
  }
  
  {
    // initialize induced known edges
    Logger::log("[Initializing Induced Known Edges]");
    #ifdef INDUCE_KNOWN_EDGES
      Logger::log("- initailize all edges!");
    #endif

    using ReasonEdge = std::tuple<int, int, std::pair<int, int>, bool>; // (from, to, reason, is_rw)
    struct CmpReasonEdge {
      auto operator() (const ReasonEdge &lhs, const ReasonEdge &rhs) const -> bool {
        return lhs < rhs;
      };
    };
    // known_induced_edges_set will ensures duplicate-free reasonedges
    using known_induced_edges_set = std::set<ReasonEdge, CmpReasonEdge>;
    auto known_induced_edges_set_of = std::vector<known_induced_edges_set>(polygraph->n_vars);

    for (int v = 0; v < polygraph->n_vars; v++) {
      Logger::log(fmt::format("var {}:", v));
      auto &s = known_induced_edges_set_of[v];
      if (polygraph->is_ww_var(v)) {
        const auto &[from, to, keys] = polygraph->ww_info[v];
        s.insert(ReasonEdge{from, to, {v, -1}, /* is_rw = */ false});
        Logger::log(fmt::format(" - WW: {} -> {}, reason = ({}, {}), Known(Itself)", from, to, v, -1));
      } else if (polygraph->is_wr_var(v)) {
        const auto &[from, to, key] = polygraph->wr_info[v];
        s.insert(ReasonEdge{from, to, {-1, v}, /* is_rw = */ false});
        Logger::log(fmt::format(" - WR: {} -> {}, reason = ({}, {}), Known(Itself)", from, to, -1, v));
      } else {
        assert(false);
      }

      #ifdef INDUCE_KNOWN_EDGES
        // copied from add_edges_of_var()
        const auto &var = v;
        if (polygraph->is_ww_var(v)) {
          const auto &[from, to, keys] = polygraph->ww_info[var];
          for (const auto &key : keys) {
            if (wr_to[from][key].empty()) continue;
            for (const auto &to2 : wr_to[from][key]) {
              if (to2 == to) continue;
              auto var2 = polygraph->wr_var_of[from][to2][key];
              Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to2, to, var, var2));
              s.insert(ReasonEdge{to2, to, {var, var2}, /* is_rw = */ true});
            }
          }
        } else if (polygraph->is_wr_var(v)) {
          const auto &[from, to, key] = polygraph->wr_info[var];
          for (const auto &to2 : ww_to[from][key]) {
            if (to2 == to) continue;
            auto var2 = polygraph->ww_var_of[from][to2];
            Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to, to2, var2, var));
            s.insert(ReasonEdge{to, to2, {var2, var}, /* is_rw = */ true});
          }
        } else {
          assert(false);
        }
      #endif
    }

    // copy results into known_induced_edges
    for (int v = 0; v < polygraph->n_vars; v++) {
      const auto &edges_set = known_induced_edges_set_of[v];
      auto &edges = known_induced_edges_of[v];
      for (const auto &e :edges_set) {
        edges.emplace_back(e);
      } 
    }

    #ifdef INDUCE_KNOWN_EDGES
      // clear known edges
      ww_to.assign(polygraph->n_vertices, {});
      wr_to.assign(polygraph->n_vertices, {});
    #endif
  }

  // * note: assume known graph has reached a fixed point, 
  // * i.e. no more RW edge can be induced here
  // TODO: check acyclicity of known graph and report possible conflict

  // ! deprecated
  // initialize vars_heap, sorting by n_edges_of_var
  int n_vars = polygraph->n_vars;
  for (int i = 0; i < n_vars; i++) vars_heap.insert({known_induced_edges_of[i].size(), i});

  assert(icd_graph.preprocess());
}

void AcyclicSolverHelper::add_var(int var) {
  // add_var adds the var into vars_heap
  int n_edges = known_induced_edges_of[var].size();
  vars_heap.insert({n_edges, var});
  icd_graph.set_var_assigned(var, false);
} 

void AcyclicSolverHelper::remove_var(int var) {
  // remove_var removes the var from vars_heap
  int n_edges = known_induced_edges_of[var].size();
  if (vars_heap.contains({n_edges, var})) {
    vars_heap.erase({n_edges, var});
    icd_graph.set_var_assigned(var, true);
  }
}

bool AcyclicSolverHelper::add_edges_of_var(int var) { 
  // return true if edge is successfully added into the graph, i.e. no cycle is detected 
  auto &added_edges = added_edges_of[var];
  assert(added_edges.empty());

  bool cycle = false;
  if (polygraph->is_ww_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WW", var));
    // 1. add induced known edges, including itself(ww)
    // if INDUCE_KNOWN_EDGES is enabled, known_induced_known_edges_of[var] contains all induced edges(including itself)
    // otherwise, it will only contain itself(ww)
    const auto &known_induced_edges = known_induced_edges_of[var];
    for (const auto &[from, to, reason, is_rw] : known_induced_edges) {
      Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {}), Known", from, to, reason.first, reason.second));
      // cycle = !icd_graph.add_edge(from, to, reason);
      const auto &[reason1, reason2] = reason;
      cycle = !(is_rw ? add_induced_anti_dep_edge(var, from, to, reason1, reason2) : add_induced_dep_edge(var, from, to, var));
      // except itself, known_induced_edges contains all rw edges
      if (cycle) {
        Logger::log(" - conflict!");
        goto conflict; // bad implementation
      } 
      Logger::log(" - success");
      added_edges.push({from, to, reason, is_rw});
    }
    
    // 2. add induced rw edges
    const auto &[from, to, keys] = polygraph->ww_info[var];
    for (const auto &key : keys) {
      if (wr_to[from][key].empty()) continue;
      for (const auto &to2 : wr_to[from][key]) {
        if (to2 == to) continue;
        auto var2 = polygraph->wr_var_of[from][to2][key];
        Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to2, to, var, var2));
        // cycle = !icd_graph.add_edge(to2, to, {var, var2});
        cycle = !add_induced_anti_dep_edge(var, to2, to, var, var2);
        if (cycle) {
          Logger::log(" - conflict!");
          goto conflict; // bad implementation
        } 
        Logger::log(" - success");
        added_edges.push({to2, to, {var, var2}, /* is_rw = */ true});
      }
    }

    if (!cycle) {
      for (const auto &key : keys) {
        ww_to[from][key].insert(to);
        Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into ww_to, now ww_to[{}][{}] = {} ", from, to, key, from, key, Logger::urdset2str(ww_to[from][key])));
      }
    } 
  } else if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- adding {}, type = WR", var));

    // 1. add known induced edges, including itself(wr)
    // see ww branch for more info
    const auto &known_induced_edges = known_induced_edges_of[var];
    for (const auto &[from, to, reason, is_rw] : known_induced_edges) {
      Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {}), Known", from, to, reason.first, reason.second));
      // cycle = !icd_graph.add_edge(from, to, reason);
      const auto &[reason1, reason2] = reason;
      cycle = !(is_rw ? add_induced_anti_dep_edge(var, from, to, reason1, reason2) : add_induced_dep_edge(var, from, to, var));
      if (cycle) {
        Logger::log(" - conflict!");
        goto conflict; // bad implementation
      } 
      Logger::log(" - success");
      added_edges.push({from, to, reason, is_rw});
    }

    // 2. add induced rw edges
    const auto &[from, to, key] = polygraph->wr_info[var];
    for (const auto &to2 : ww_to[from][key]) {
      if (to2 == to) continue;
      auto var2 = polygraph->ww_var_of[from][to2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {}), Induced", to, to2, var2, var));
      // cycle = !icd_graph.add_edge(to, to2, {var2, var});
      cycle = !add_induced_anti_dep_edge(var, to, to2, var2, var);
      if (cycle) {
        Logger::log(" - conflict!");
        break;
      } 
      Logger::log(" - success");
      added_edges.push({to, to2, {var2, var}, /* is_rw = */ true});
    }

    if (!cycle) {
      wr_to[from][key].insert(to);
      Logger::log(fmt::format(" - inserting ({} -> {}, key = {}) into wr_to, now wr_to[{}][{}] = {}", from, to, key, from, key, Logger::urdset2str(wr_to[from][key])));
    } 
  } else {
    assert(false);
  }

  if (!cycle) {
    Logger::log(fmt::format(" - {} is successfully added", var));
    icd_graph.get_propagated_lits(propagated_lits);
    construct_wr_cons_propagated_lits(var);
    return true;
  } 

  conflict:
    Logger::log(fmt::format(" - conflict found! reverting adding edges of {}", var));

    // generate conflict clause
    // std::vector<Lit> cur_conflict_clause;
    // icd_graph.get_minimal_cycle(cur_conflict_clause);

    // for (Lit l : cur_conflict_clause) std::cerr << l.x << " ";
    // std::cerr << std::endl;

    // conflict_clauses.emplace_back(cur_conflict_clause);

    while (!added_edges.empty()) {
      const auto &[from, to, reason, is_rw] = added_edges.top();
      Logger::log(fmt::format(" - removing edge {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
      // icd_graph.remove_edge(from, to, reason);
      if (is_rw) {
        const auto &[ww_reason, wr_reason] = reason;
        remove_induced_anti_dep_edge(var, from, to, ww_reason, wr_reason); 
      } else {
        remove_induced_dep_edge(var, from, to, var);
      }
      added_edges.pop();
    }
    Logger::log(fmt::format(" - {} is not been added", var));
    assert(added_edges.empty());
    return false;
} 

void AcyclicSolverHelper::remove_edges_of_var(int var) {
#ifndef ADDED_EDGES_CACHE
  std::cerr << "Case of undefined ADDED_EDGES_CACHE for SI checking is not implemented yet!"
  assert(0);
  if (polygraph->is_ww_var(var)) {
    Logger::log(fmt::format("- removing {}, type = WW", var));
    const auto &[from, to, keys] = polygraph->ww_info[var];
    // 1. remove itself, ww
    Logger::log(fmt::format(" - WW: {} -> {}, reason = ({}, {})", from, to, var, -1));
    icd_graph.remove_edge(from, to, {var, -1});
    // 2. remove induced rw edges
    for (const auto &[to2, key2] : wr_to[from]) {
      if (to2 == to) continue;
      if (!keys.contains(key2)) continue;
      // RW: to2 -> to
      auto var2 = polygraph->wr_var_of[from][to2][key2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to2, to, var, var2));
      icd_graph.remove_edge(to2, to, {var, var2});
    }
    assert(ww_to[from].contains(to));
    ww_to[from].erase(to);
    Logger::log(fmt::format(" - deleting {} -> {} into ww_to, now ww_to[{}] = {} ", from, to, from, Logger::urdset2str(ww_to[from])));
  } else if (polygraph->is_wr_var(var)) {
    Logger::log(fmt::format("- removing {}, type = WR", var));
    const auto &[from, to, key] = polygraph->wr_info[var];
    // 1. remove itself, wr
    Logger::log(fmt::format(" - WR: {} -> {}, reason = ({}, {})", from, to, -1, var));
    icd_graph.remove_edge(from, to, {-1, var});
    // 2. add induced rw edges
    for (const auto &to2 : ww_to[from]) {
      if (to2 == to) continue;
      const auto &key2s = ww_keys[from][to2];
      if (!key2s.contains(key)) continue;
      // RW: to -> to2
      auto var2 = polygraph->ww_var_of[from][to2];
      Logger::log(fmt::format(" - RW: {} -> {}, reason = ({}, {})", to, to2, var2, var));
      icd_graph.remove_edge(to, to2, {var2, var});
    }
    assert(wr_to[from].contains({to, key}));
    wr_to[from].erase({to, key});
    Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into wr_to, now wr_to[{}] = {}", from, to, key, from, Logger::wr_to2str(wr_to[from])));
  } else {
    assert(false);
  }
  while (!added_edges_of[var].empty()) add_edges_of_var[var].pop();

#else

  Logger::log(fmt::format("- removing {}, type = {}", var, (polygraph->is_ww_var(var) ? "WW" : "WR")));

  auto &added_edges = added_edges_of[var];
  while (!added_edges.empty()) {
    const auto &[from, to, reason, is_rw] = added_edges.top();
    // icd_graph.remove_edge(from, to, reason);
    if (is_rw) {
      const auto &[ww_reason, wr_reason] = reason;
      remove_induced_anti_dep_edge(var, from, to, ww_reason, wr_reason);
    } else {
      remove_induced_dep_edge(var, from, to, var);
    }
    Logger::log(fmt::format(" - ??: {} -> {}, reason = ({}, {})", from, to, reason.first, reason.second));
    added_edges.pop();
  }
  // all edges have been deleted including the WW or WR itself
  if (polygraph->is_ww_var(var)) {
    const auto &[from, to, keys] = polygraph->ww_info[var];
    for (const auto &key : keys) {
      assert(ww_to[from][key].contains(to));
      ww_to[from][key].erase(to);
      Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into ww_to, now ww_to[{}][{}] = {} ", from, to, key, from, key, Logger::urdset2str(ww_to[from][key])));
    }
  } else if (polygraph->is_wr_var(var)) { 
    const auto &[from, to, key] = polygraph->wr_info[var];
    assert(wr_to[from][key].contains(to));
    wr_to[from][key].erase(to);
    Logger::log(fmt::format(" - deleting ({} -> {}, key = {}) into wr_to, now wr_to[{}][{}] = {}", from, to, key, from, key, Logger::urdset2str(wr_to[from][key])));
  } else {
    assert(false);
  }

#endif

  assert(added_edges_of[var].empty());
}

bool AcyclicSolverHelper::add_induced_dep_edge(int var, int from, int to, int dep_reason) {
  if (var == -1) { // known
    assert(dep_reason == -1);
    add_known_induced_dep_edge(from, to);
    return true;
  }
  auto added_induced_edges = std::stack<std::tuple<int, int, std::tuple<int, int, int>>> {}; // (from, to, reason)
  bool cycle = false;
  // 1. add itself
  cycle = !icd_graph.add_edge(from, to, {dep_reason, -1, -1}); 
  if (cycle) goto inner_dep_conflict;
  added_induced_edges.push({from, to, {dep_reason, -1, -1}});
  // 2. add induced edges
  for (const auto &[rw_to, ww_reason, wr_reason] : anti_dep_edges_of[to]) {
    cycle = !icd_graph.add_edge(from, rw_to, {dep_reason, ww_reason, wr_reason});
    if (rw_to == from) assert(cycle); // selfloops directly drives to conflict
    if (cycle) goto inner_dep_conflict; // bad implementation!
    added_induced_edges.push({from, rw_to, {dep_reason, ww_reason, wr_reason}});
  }
  dep_edges_of[to].insert({from, dep_reason});
  return true;

  inner_dep_conflict:
    // generate conflict clause
    std::vector<Lit> cur_conflict_clause;
    icd_graph.get_minimal_cycle(cur_conflict_clause);

    // for (Lit l : cur_conflict_clause) std::cerr << l.x << " ";
    // std::cerr << std::endl;

    conflict_clauses.emplace_back(cur_conflict_clause);
    while (!added_induced_edges.empty()) {
      const auto &[from, to, reason] = added_induced_edges.top();
      icd_graph.remove_edge(from, to, reason);
      added_induced_edges.pop();
    }
    return false;
}

void AcyclicSolverHelper::add_known_induced_dep_edge(int from, int to) {
  // acyclicity test is preprocessed by the last pruning pass
  icd_graph.add_known_edge(from, to); 
  for (const auto &[rw_to, ww_reason, wr_reason] : anti_dep_edges_of[to]) {
    assert(ww_reason == -1 && wr_reason == -1); // only have known edges when handling known graph
    icd_graph.add_known_edge(from, rw_to);
    assert(rw_to != from); // selfloops directly drives to conflict
  }
  dep_edges_of[to].insert({from, /* dep_reason = */ -1});
}

bool AcyclicSolverHelper::add_induced_anti_dep_edge(int var, int from, int to, int ww_reason, int wr_reason) {
  if (var == -1) {
    assert(ww_reason == -1 && wr_reason == -1);
    add_known_induced_anti_dep_edge(from, to);
    return true;
  }
  auto added_induced_edges = std::stack<std::tuple<int, int, std::tuple<int, int, int>>> {}; // (from, to, reason)
  bool cycle = false;
  // 1. only add induced edges except itself
  for (const auto &[dep_from, dep_reason] : dep_edges_of[from]) {
    cycle = !icd_graph.add_edge(dep_from, to, {dep_reason, ww_reason, wr_reason});
    if (dep_from == to) assert(cycle); // selfloop directly drives to conflict
    if (cycle) goto inner_anti_dep_conflict; // bad implementation!
    added_induced_edges.push({dep_from, to, {dep_reason, ww_reason, wr_reason}});
  }
  anti_dep_edges_of[from].insert({to, ww_reason, wr_reason});
  return true;

  inner_anti_dep_conflict:
    // generate conflict clause
    std::vector<Lit> cur_conflict_clause;
    icd_graph.get_minimal_cycle(cur_conflict_clause);

    // for (Lit l : cur_conflict_clause) std::cerr << l.x << " ";
    // std::cerr << std::endl;

    conflict_clauses.emplace_back(cur_conflict_clause);
    while (!added_induced_edges.empty()) {
      const auto &[from, to, reason] = added_induced_edges.top();
      icd_graph.remove_edge(from, to, reason);
      added_induced_edges.pop();
    }
    return false;
}

void AcyclicSolverHelper::add_known_induced_anti_dep_edge(int from, int to) {
  // acyclicity test is preprocessed by the last pruning pass
  // only add induced edges except itslef
  for (const auto &[dep_from, dep_reason] : dep_edges_of[from]) {
    assert(dep_reason == -1); // only have known edges when handling known graph
    icd_graph.add_known_edge(dep_from, to);
    assert(dep_from != to); // selfloop directly drives to conflict
  }
  anti_dep_edges_of[from].insert({to, /* ww_reason = */ -1, /* wr_reason = */ -1});
}

void AcyclicSolverHelper::remove_induced_dep_edge(int var, int from, int to, int dep_reason) {
  assert(var >= 0); // var != -1
  // 1. remove itself
  icd_graph.remove_edge(from, to, {dep_reason, -1, -1}); 
  // 2. remove induced edges
  for (const auto &[rw_to, ww_reason, wr_reason] : anti_dep_edges_of[to]) {
    icd_graph.remove_edge(from, rw_to, {dep_reason, ww_reason, wr_reason});
  }
  dep_edges_of[to].erase(dep_edges_of[to].find({from, dep_reason}));
}

void AcyclicSolverHelper::remove_induced_anti_dep_edge(int var, int from, int to, int ww_reason, int wr_reason) {
  assert(var >= 0); // var != -1
  // 1. only remove induced edges
  for (const auto &[dep_from, dep_reason] : dep_edges_of[from]) {
    icd_graph.remove_edge(dep_from, to, {dep_reason, ww_reason, wr_reason});
  }
  anti_dep_edges_of[from].erase(anti_dep_edges_of[from].find({to, ww_reason, wr_reason}));
}

Var AcyclicSolverHelper::get_var_represents_max_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = --vars_heap.end();
  return Var(it->second);
}

Var AcyclicSolverHelper::get_var_represents_min_edges() {
  if (vars_heap.empty()) return var_Undef;
  auto it = vars_heap.begin();
  return Var(it->second);
}

void AcyclicSolverHelper::construct_wr_cons_propagated_lits(int var) {
  // construct wr constraint propagated lits
#ifdef ENABLE_WRCP
  if (!polygraph->is_wr_var(var)) return;
  Logger::log(fmt::format("- [Construct WRCP of {}]", var));
  auto wr_cons_ref = polygraph->get_wr_cons(var);
  if (!wr_cons_ref) {
    Logger::log(fmt::format("- unit wr cons {}, end WRCP", var));
    return;
  }
  for (const auto &var2 : *wr_cons_ref) {
    if (icd_graph.get_var_assigned(var2)) continue;
    Logger::log(fmt::format(" - prop (~{}) with reason (~{} | ~{})", var2, var, var2));
    propagated_lits.emplace_back(std::pair<Lit, std::vector<Lit>>{~mkLit(var2), {~mkLit(var), ~mkLit(var2)}});
  }
  Logger::log(fmt::format("- end of WRCP construction", var));
#endif
}

Polygraph *AcyclicSolverHelper::get_polygraph() { return polygraph; }

const int AcyclicSolverHelper::get_level(int x) const { return icd_graph.get_level(x); }

namespace Logger {

// ! This is a VERY BAD implementation, see Logger.h for more details
auto wr_to2str(const std::unordered_set<std::pair<int, int64_t>, decltype(pair_hash_endpoint)> &s) -> std::string {
  if (s.empty()) return std::string{"null"};
  auto os = std::ostringstream{};
  for (const auto &[x, y] : s) {
    // std::cout << std::to_string(i) << std::endl;
    os << "{" << std::to_string(x) << ", " << std::to_string(y) << "}, ";
  } 
  auto ret = os.str();
  ret.erase(ret.length() - 2); // delete last ", "
  // std::cout << "\"" << ret << "\"" << std::endl;
  return ret; 
}

} // namespace MinisatSI::Logger

} // namespace MinisatSI