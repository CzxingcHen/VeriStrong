#ifndef MINISAT_MONITOR_H
#define MINISAT_MONITOR_H

#include <cstdint>
#include <unordered_map>
#include <chrono>

namespace Minisat {
class Monitor {
public:
  int find_cycle_times;
  int propagated_lit_times;
  int64_t add_edge_times;
  int64_t add_edge_in_icd_graph_times;
  int64_t dfs_when_finding_cycle_in_icd_graph_times;
  int64_t find_cycle_in_icd_graph_times;

  int64_t dfs_m_times = 0;

  int64_t extend_times;
  int64_t cycle_edge_count_sum;
  std::unordered_map<int, int64_t> cycle_width_count;
  std::unordered_map<int, int64_t> minimal_cycle_width_count;
  int skipped_bridge_count;
  int64_t construct_uep_count;
  int64_t uep_b_size_sum, uep_f_size_sum;
  int64_t propagated_lit_add_times;  

  int64_t n_contributed_swaps;

  double var_divide_known_edge_ratio_sum;

  std::chrono::milliseconds encode_time;
  std::chrono::milliseconds rw_derivation_and_cycle_detection_time;
  std::chrono::milliseconds cycle_detection_time;
  std::chrono::milliseconds theory_propagation_time;
  std::chrono::milliseconds propagate_time;
  std::chrono::milliseconds search_time;

  static Monitor *monitor;

  Monitor();
  static Monitor *get_monitor();
  void show_statistics();
};

// TODO: implement more monitor options


} // namespace Minisat::Monitor

#endif // MINISAT_MONITOR_H