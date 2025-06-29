#include "Monitor.h"

#include <iostream>

namespace MinisatSI {

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
}

Monitor *Monitor::get_monitor() {
  if (monitor == nullptr) monitor = new Monitor();
  return monitor;
}

void Monitor::show_statistics() {
  std::cerr << "[Monitor]" << "\n";
  std::cerr << "find_cycle_times: " << find_cycle_times << "\n";
  // std::cerr << "propagated_lit_times: " << propagated_lit_times << "\n";
  // std::cerr << "add_edge_times: " << add_edge_times << "\n";
  // std::cerr << "extend_times: " << extend_times << "\n";
  // std::cerr << "#skipped bridge = " << skipped_bridge_count << "\n";
  if (find_cycle_times != 0) std::cerr << "avg cycle length = " << 1.0 * cycle_edge_count_sum / find_cycle_times << "\n";
  // std::cerr << "#construct unit-edge propagation times = " << construct_uep_count << "\n";
  // if (construct_uep_count != 0) {
  //   std::cerr << "avg b size = " << 1.0 * uep_b_size_sum / construct_uep_count << ", "
  //             << "avg f size = " << 1.0 * uep_f_size_sum / construct_uep_count << "\n";
  // }
  // std::cerr << "#propagated lits add times = " << propagated_lit_add_times << std::endl;

  int max_width = 0;
  for (const auto &[width, count] : cycle_width_count) {
    std::cerr << "width = " << width << ", count = " << count << std::endl;
    max_width = std::max(max_width, width);
  }
  std::cerr << std::endl;
}

} // namespace MinisatSI::Monitor