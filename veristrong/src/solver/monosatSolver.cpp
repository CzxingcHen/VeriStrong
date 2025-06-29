#include "monosatSolver.h"

#include <Monosat.h>

#include <vector>
#include <string>
#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <filesystem>

#include <boost/log/trivial.hpp>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "utils/log.h"
#include "utils/gnf.h"

namespace fs = std::filesystem;

namespace checker::solver {

MonosatSolver::MonosatSolver(const history::DependencyGraph &known_graph,
                             const history::Constraints &constraints,
                             const history::HistoryMetaInfo &history_meta_info,
                             const std::string &isolation_level,
                             const bool _enable_unique_encoding = false) {
  if (isolation_level == "si") {
    std::cerr << "Not implemented yet!" << std::endl;
    throw std::runtime_error{"MonosatSolver is not supporting SI checking now"};
  }

  enable_unique_encoding = _enable_unique_encoding;

  auto unique_filename = []() -> std::string {
    auto currentTime = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();

    auto name = std::string{"mono-"} + std::to_string(timestamp) + std::string{".gnf"};
    return name;
  };
  
  solver = newSolver();
  gnf_path = fs::temp_directory_path();
  gnf_path = gnf_path / unique_filename();
  try {
    utils::write_to_gnf_file(gnf_path, known_graph, constraints, enable_unique_encoding);
  } catch (std::runtime_error &e) {
    // TODO
  }
}

// MonosatSolver::MonosatSolver(const history::DependencyGraph &known_graph,
//                             const std::vector<history::Constraint> &constraints) {
//   solver = newSolver();
//   gnf_path = fs::current_path();
//   gnf_path.append("monosat_tmp_input.gnf");
//   try {
//     utils::write_to_gnf_file(gnf_path, known_graph, constraints);
//   } catch (std::runtime_error &e) {
//     // TODO
//   };
//   BOOST_LOG_TRIVIAL(debug)
//   << "generating tmp file "
//   << gnf_path; 
// }

auto MonosatSolver::solve() -> bool {
  // auto read_file_st_time = std::chrono::steady_clock::now();
  readGNF(solver, gnf_path.c_str());
  // auto read_file_ed_time = std::chrono::steady_clock::now();

  // BOOST_LOG_TRIVIAL(info) << "read .gnf time: " << std::chrono::duration_cast<std::chrono::milliseconds>(read_file_ed_time - read_file_st_time);
  // std::cout << gnf_path << std::endl;
  bool ret = solveWrapper(solver);
  return ret;
}

MonosatSolver::~MonosatSolver() { 
  deleteSolver(solver); 
  try {
    bool deleted_tmp_file = fs::remove(gnf_path);
    if (!deleted_tmp_file) std::cerr << "tmp file is not correctly deleted." << std::endl;
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Error deleting file: " << e.what() << '\n';
  }
};

}