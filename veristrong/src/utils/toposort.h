#ifndef CHECKER_UTILS_TOPOSORT_H
#define CHECKER_UTILS_TOPOSORT_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/literal.h"
#include "utils/ranges.h"
#include "utils/to_container.h"

namespace checker::utils {

template <typename Vertex>
struct TopologicalOrder {
  static_assert(std::is_integral_v<Vertex>);
  std::vector<Vertex> pos_to_vertex;
  std::vector<size_t> vertex_to_pos;

  explicit TopologicalOrder(const std::ranges::range auto &vertices)
      : pos_to_vertex{std::ranges::begin(vertices),
                      std::ranges::end(vertices)} {
    vertex_to_pos.resize(pos_to_vertex.size());

    for (auto i = 0_uz; i < pos_to_vertex.size(); i++) {
      vertex_to_pos.at(pos_to_vertex.at(i)) = i;
    }
  }

  auto vertex_pos(Vertex v) const -> size_t { return vertex_to_pos.at(v); }

  auto vertex_at_pos(size_t index) const -> Vertex {
    return pos_to_vertex.at(index);
  }

  auto update_range(const std::vector<Vertex> &partial_order,
                    size_t start_pos) {
#ifndef NDEBUG
    auto original_vertices =
        std::ranges::views::counted(pos_to_vertex.begin() + start_pos,
                                    partial_order.size()) |
        to<std::unordered_set<size_t>>;

    auto new_vertices = partial_order | to<std::unordered_set<size_t>>;

    assert(original_vertices == new_vertices);
#endif

    auto pos = start_pos;
    for (const auto &v : partial_order) {
      pos_to_vertex.at(pos) = v;
      vertex_to_pos.at(v) = pos++;
    }
  }

  auto vertices() const -> std::ranges::range auto{
    return std::ranges::views::all(pos_to_vertex);
  }
};

template <typename Graph>
struct IncrementalCycleDetector {
  using Vertex = typename Graph::vertex_descriptor;
  using Edge = typename Graph::edge_descriptor;

  Graph *graph;
  TopologicalOrder<typename Graph::vertex_descriptor> topo_order;

  size_t n_bfs_called = 0;

  explicit IncrementalCycleDetector(Graph &graph)
      : graph{&graph}, topo_order{[&] {
          auto [begin, end] = boost::vertices(graph);
          return std::ranges::subrange(begin, end);
        }()} {}

  // ref:
  // http://www.doc.ic.ac.uk/%7Ephjk/Publications/DynamicTopoSortAlg-JEA-07.pdf
  auto check_add_edge(const Edge &edge) -> std::optional<std::vector<Edge>> {
    using boost::depth_first_search;
    using boost::filtered_graph;
    using boost::keep_all;
    using boost::source;
    using boost::target;
    using std::back_inserter;
    using std::function;
    using std::greater;
    using std::max;
    using std::min;
    using std::nullopt;
    using std::numeric_limits;
    using std::pair;
    using std::vector;
    using std::ranges::reverse;
    using std::ranges::sort;
    using std::ranges::subrange;
    using std::ranges::views::iota;
    using std::ranges::views::transform;

    auto from_pos = topo_order.vertex_pos(source(edge, *graph));
    auto to_pos = topo_order.vertex_pos(target(edge, *graph));
    if (from_pos < to_pos) {
      return nullopt;
    }

    auto filter_vertices = [&](Vertex vertex) {
      auto pos = topo_order.vertex_pos(vertex);
      return to_pos <= pos && pos <= from_pos;
    };

    auto partial_graph = filtered_graph{
        *graph,
        keep_all{},
        function{filter_vertices},
    };

    auto [partial_topo_order, cycle] = bfs_topo_sort(edge, partial_graph);
    assert(!cycle.empty() || !partial_topo_order.empty());
    if (!cycle.empty()) {
      return {std::move(cycle)};
    }

    topo_order.update_range(partial_topo_order, to_pos);
    return nullopt;
  }

  auto bfs_topo_sort(const Edge &added_edge, const auto &filtered_graph)
      -> std::pair<std::vector<Vertex>, std::vector<Edge>> {
    using boost::edge;
    using boost::num_vertices;
    using boost::out_edges;
    using boost::source;
    using boost::target;
    using boost::vertices;
    using std::numeric_limits;
    using std::pair;
    using std::queue;
    using std::vector;
    using std::ranges::find_if;
    using std::ranges::reverse;

    n_bfs_called++;

    auto from = source(added_edge, filtered_graph);
    auto to = target(added_edge, filtered_graph);

    auto topo_order = vector<Vertex>{};
    auto q = queue<Vertex>{};
    auto in_degree = vector<size_t>(num_vertices(filtered_graph));

    for (auto &&v : as_range(vertices(filtered_graph))) {
      for (auto &&e : as_range(out_edges(v, filtered_graph))) {
        in_degree.at(target(e, filtered_graph))++;
      }
    }
    for (auto &&v : as_range(vertices(filtered_graph))) {
      if (!in_degree.at(v)) {
        q.push(v);
      }
    }

    while (!q.empty()) {
      auto v = q.front();
      q.pop();
      topo_order.emplace_back(v);

      for (auto &&e : as_range(out_edges(v, filtered_graph))) {
        auto v2 = target(e, filtered_graph);

        if (!--in_degree.at(v2)) {
          q.push(v2);
        }
      }
    }

    if (find_if(in_degree, [](auto n) { return n != 0; }) == in_degree.end()) {
      return pair{std::move(topo_order), vector<Edge>{}};
    }

    auto predecessor = vector<Vertex>(num_vertices(filtered_graph),
                                      numeric_limits<Vertex>::max());
    q.push(to);
    while (true) {
      assert(!q.empty());
      auto v = q.front();
      q.pop();

      if (v == from) {
        break;
      }

      for (auto &&e : as_range(out_edges(v, filtered_graph))) {
        auto v2 = target(e, filtered_graph);
        if (predecessor.at(v2) != numeric_limits<Vertex>::max()) {
          continue;
        }

        predecessor.at(v2) = v;
        q.push(v2);
      }
    }

    auto cycle = vector{added_edge};
    for (auto v = from; v != to; v = predecessor.at(v)) {
      auto [e, has_edge] = edge(predecessor.at(v), v, filtered_graph);
      assert(has_edge);
      cycle.emplace_back(e);
    }

    reverse(cycle);
    return pair{vector<Vertex>{}, std::move(cycle)};
  }

  ~IncrementalCycleDetector() {
    BOOST_LOG_TRIVIAL(debug) << "bfs() called " << n_bfs_called << " times";
  }
};

}  // namespace checker::utils

#endif  // CHECKER_UTILS_TOPOSORT_H
