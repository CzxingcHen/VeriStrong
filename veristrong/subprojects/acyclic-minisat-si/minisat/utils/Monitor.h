#ifndef MINISAT_SI_MONITOR_H
#define MINISAT_SI_MONITOR_H

#include <cstdint>
#include <unordered_map>

namespace MinisatSI {
class Monitor {
public:
  int find_cycle_times;
  int propagated_lit_times;
  int64_t add_edge_times;
  int64_t extend_times;
  int64_t cycle_edge_count_sum;
  int skipped_bridge_count;
  int64_t construct_uep_count;
  int64_t uep_b_size_sum, uep_f_size_sum;
  int64_t propagated_lit_add_times;  

  std::unordered_map<int, int64_t> cycle_width_count;
  std::unordered_map<int, int64_t> minimal_cycle_width_count;

  static Monitor *monitor;

  Monitor();
  static Monitor *get_monitor();
  void show_statistics();
};

// TODO: implement more monitor options


} // namespace MinisatSI::Monitor

#endif // MINISAT_SI_MONITOR_H