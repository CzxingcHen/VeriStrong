#ifndef CHECKER_UTILS_AGNF_H
#define CHECKER_UTILS_AGNF_H

#include <vector>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <cassert>

#include <boost/log/trivial.hpp>

#include "history/constraint.h"
#include "history/dependencygraph.h"

namespace fs = std::filesystem;

namespace checker::utils {
// void write_to_agnf_file(fs::path &agnf_path, 
//                         const history::DependencyGraph &known_graph,
//                         const std::vector<history::Constraint> &constraints) {
//   using AGNFEdge = std::pair<int64_t, int64_t>;
//   constexpr auto hash_gnf_edge_endpoint = [](const auto &t) {
//     auto [t1, t2] = t;
//     std::hash<int64_t> h;
//     return h(t1) ^ h(t2);
//   };
//   std::unordered_map<AGNFEdge, int64_t, decltype(hash_gnf_edge_endpoint)> edge_id;
//   std::unordered_map<int64_t, AGNFEdge> id_edge;
//   std::unordered_map<int64_t, int64_t> node_id;
//   int64_t n_edge = 0, n_node = 0;
//   auto alloc_edge_id = [&](const auto &edge) mutable {
//     if (edge_id.contains(edge)) return;
//     edge_id[edge] = ++n_edge;
//     id_edge[n_edge] = edge;
//   };
//   auto alloc_node_id = [&](const auto &node) mutable {
//     if (node_id.contains(node)) return;
//     node_id[node] = n_node++;
//   };
//   auto alloc_edge = [&](const auto &from, const auto &to) {
//     alloc_edge_id((AGNFEdge) {from, to});
//     alloc_node_id(from), alloc_node_id(to);
//   };
//   for (const auto &[from, to, _] : known_graph.edges()) { 
//     alloc_edge(from, to); 
//   } 
//   int64_t n_known_edge = n_edge;
//   for (const auto &cons : constraints) {
//     for (const auto &[from, to, _] : cons.either_edges) alloc_edge(from, to);
//     for (const auto &[from, to, _] : cons.or_edges) alloc_edge(from, to); 
//   }
  
//   std::ofstream ofs;
//   ofs.open(agnf_path, std::ios::out | std::ios::trunc);
//   ofs << n_node << " " << n_edge << " " << n_known_edge << " " << constraints.size() << std::endl;
//   for (int64_t id = 1; id <= n_edge; id++) {
//     auto &[from, to] = id_edge[id];
//     ofs << node_id[from] << " " << node_id[to] << std::endl;
//   }
//   std::set<int64_t> output_known_edge_ids;
//   int64_t known_edge_cnt = 0;
//   for (const auto &[from, to, _] : known_graph.edges()) {
//     auto edge = std::make_pair(from, to);
//     if (!edge_id.contains(edge)) {
//       BOOST_LOG_TRIVIAL(info) << "Oops! not allocate edge if to (from, to)";
//       continue;
//     }
//     auto eid = edge_id[edge];
//     if (output_known_edge_ids.contains(eid)) continue;
//     output_known_edge_ids.insert(eid);
//     ofs << node_id[from] << " " << node_id[to] << std::endl;
//     ++known_edge_cnt;
//   }
//   assert(known_edge_cnt == n_known_edge);
//   for (const auto &cons : constraints) {
//     for (const auto &[from, to, _] : cons.either_edges) {
//       if (!edge_id.contains(std::make_pair(from, to))) {
//         BOOST_LOG_TRIVIAL(info) << "Oops! not allocate edge if to (from, to)";
//         continue;
//       }
//       auto eid = edge_id[std::make_pair(from, to)]; // edge_id of (from, to)
//       ofs << eid << " ";
//     }
//     ofs << 0 << std::endl;

//     for (const auto &[from, to, _] : cons.or_edges) {
//       if (!edge_id.contains(std::make_pair(from, to))) {
//         BOOST_LOG_TRIVIAL(info) << "Oops! not allocate edge if to (from, to)";
//         continue;
//       }
//       auto eid = edge_id[std::make_pair(from, to)]; // edge_id of (from, to)
//       ofs << eid << " ";
//     }
//     ofs << 0 << std::endl;
//   }

//   ofs.close();
// }
} // namespace checker::utils

#endif // CHECKER_UTILS_AGNF_H