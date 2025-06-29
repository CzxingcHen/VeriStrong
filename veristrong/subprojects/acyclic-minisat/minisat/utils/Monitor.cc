#include "Monitor.h"

#include <iostream>
#include <vector>
#include <algorithm>

namespace Minisat {

Monitor *Monitor::monitor = nullptr;

Monitor::Monitor() {
  find_cycle_times = 0;
  propagated_lit_times = 0;
  add_edge_times = 0;
  extend_times = 0;
  skipped_bridge_count = 0;
  cycle_edge_count_sum = 0;
  construct_uep_count = 0;
  uep_b_size_sum = uep_f_size_sum = 0;
  propagated_lit_add_times = 0;
  cycle_width_count.clear();
  minimal_cycle_width_count.clear();
  var_divide_known_edge_ratio_sum = 0;

  dfs_when_finding_cycle_in_icd_graph_times = 0;
  add_edge_in_icd_graph_times = 0;
  find_cycle_in_icd_graph_times = 0;

  dfs_m_times = 0;

  encode_time = std::chrono::milliseconds::zero();
  rw_derivation_and_cycle_detection_time = std::chrono::milliseconds::zero();
  cycle_detection_time = std::chrono::milliseconds::zero();
  theory_propagation_time = std::chrono::milliseconds::zero();
  propagate_time = std::chrono::milliseconds::zero();
  search_time = std::chrono::milliseconds::zero();

  n_contributed_swaps = 0;
}

Monitor *Monitor::get_monitor() {
  if (monitor == nullptr) monitor = new Monitor();
  return monitor;
}

void Monitor::show_statistics() {
  std::cerr << "[Monitor]" << "\n";
  std::cerr << "find_cycle_times: " << find_cycle_times << "\n";
  // std::cerr << "add_edge_times: " << add_edge_times << "\n";
  // std::cerr << "propagated_lit_times: " << propagated_lit_times << "\n";
  // std::cerr << "extend_times: " << extend_times << "\n";
  // std::cerr << "#skipped bridge = " << skipped_bridge_count << "\n";
  if (find_cycle_times != 0) std::cerr << "avg cycle length = " << 1.0 * cycle_edge_count_sum / find_cycle_times << "\n";
  int max_width = 0;
  for (const auto &[width, count] : cycle_width_count) {
    std::cerr << "width = " << width << ", count = " << count << std::endl;
    max_width = std::max(max_width, width);
  }
  std::cerr << std::endl;

  std::cerr << "[Minimal]" << std::endl;
  for (const auto &[width, count] : minimal_cycle_width_count) {
    std::cerr << "width = " << width << ", count = " << count << std::endl;
  }
  std::cerr << std::endl;
  
  std::cerr << std::endl;
  std::cerr << "[Suffix Sum]" << std::endl;
  int sum = 0;
  std::vector<int> suf_sum;
  for (int i = max_width; i >= 1; i--) {
    sum += cycle_width_count[i];
    suf_sum.emplace_back(sum);
  }
  std::reverse(suf_sum.begin(), suf_sum.end());
  for (const int x : suf_sum) std::cerr << x << std::endl;
  std::cerr << std::endl;
  
  // if (find_cycle_times != 0) std::cerr << "avg var_edge/total_edge in cycles = " << 1.0 * var_divide_known_edge_ratio_sum / find_cycle_times << std::endl;
  if (add_edge_times != 0) std::cerr << "find cycle times/add edge times = " << 1.0 * find_cycle_times / add_edge_times << "\n";

  std::cerr << "\n(ICD graph)\n";

  std::cerr << "#call PK algo times: " << add_edge_in_icd_graph_times << "\n";
  std::cerr << "#dfs times: " << dfs_when_finding_cycle_in_icd_graph_times << "\n";
  std::cerr << "#find cycle times: " << find_cycle_in_icd_graph_times << "\n";
  std::cerr << "#dfs iter on edge times: " << dfs_m_times << "\n";

  std::cerr << "\n";
  std::cerr << "[Time]\n";
  std::cerr << "encode time: " << encode_time.count() << "ms\n";
  std::cerr << "propagate time: " << propagate_time.count() << "ms\n";
  std::cerr << "search time: " << search_time.count() << "ms\n";
  std::cerr << "RW derivation time: " << (rw_derivation_and_cycle_detection_time - cycle_detection_time).count() << "ms\n";
  std::cerr << "cycle detection time: " << cycle_detection_time.count() << "ms\n";
  std::cerr << "theory propagation time: " << theory_propagation_time.count() << "ms\n";
  std::cerr << std::endl;

  std::cerr << "#contributed swaps: " << n_contributed_swaps << std::endl;
  std::cerr << std::endl;

  // std::cerr << "#construct unit-edge propagation times = " << construct_uep_count << "\n";
  // if (construct_uep_count != 0) {
  //   std::cerr << "avg b size = " << 1.0 * uep_b_size_sum / construct_uep_count << ", "
  //             << "avg f size = " << 1.0 * uep_f_size_sum / construct_uep_count << "\n";
  // }
  // std::cerr << "#propagated lits add times = " << propagated_lit_add_times << std::endl;
}

} // namespace Minisat::Monitor