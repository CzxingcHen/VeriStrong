#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/to_container.h"
#include "utils/toposort.h"

#define BOOST_TEST_MODULE toposort
#include <boost/test/included/unit_test.hpp>

using boost::add_edge;
using checker::utils::to;
using checker::utils::TopologicalOrder;
using std::array;
using std::initializer_list;
using std::pair;
using std::unordered_set;
using std::vector;
using std::ranges::equal;
using std::ranges::range;
using std::ranges::reverse;
using std::ranges::views::filter;

using Graph = boost::adjacency_list<>;
using Vertex = Graph::vertex_descriptor;
using Edge = Graph::edge_descriptor;
static_assert(std::is_same_v<Vertex, size_t>);

static auto create_graph(size_t num_vertices,
                         initializer_list<pair<Vertex, Vertex>> edges)
    -> pair<std::unique_ptr<Graph>,
            checker::utils::IncrementalCycleDetector<Graph>> {
  auto graph = std::make_unique<Graph>(num_vertices);
  auto detector = checker::utils::IncrementalCycleDetector{*graph};

  for (auto [from, to] : edges) {
    auto [e, _] = add_edge(from, to, *graph);
    assert(!detector.check_add_edge(e));
  }

  return {std::move(graph), std::move(detector)};
}

static auto partial_equal(const TopologicalOrder<Vertex> &topo_order,
                          const vector<Vertex> &partial_order) -> bool {
  auto in_partial_order =
      filter([s = partial_order | to<unordered_set<Vertex>>](auto v) {
        return s.contains(v);
      });

  return equal(topo_order.vertices() | in_partial_order, partial_order);
}

static auto cycle_equal(const vector<Edge> &cycle,
                        const vector<Vertex> &vertices, const Graph &graph)
    -> bool {
  auto vertex_edges = unordered_set<Edge, boost::hash<Edge>>{};
  for (auto i = 0_uz; i < vertices.size(); i++) {
    auto [edge, has_edge] = boost::edge(
        vertices.at(i), vertices.at((i + 1) % vertices.size()), graph);

    assert(has_edge);
    vertex_edges.emplace(edge);
  }

  return vertex_edges == (cycle | to<unordered_set<Edge, boost::hash<Edge>>>);
}

BOOST_AUTO_TEST_CASE(toposort) {
  auto [g, d] = create_graph(5, {{0, 1}, {1, 3}, {2, 3}});

  auto e = add_edge(1, 2, *g).first;
  BOOST_TEST(!d.check_add_edge(e).has_value());
  BOOST_TEST(partial_equal(d.topo_order, {0, 1, 2, 3}));

  auto e2 = add_edge(3, 4, *g).first;
  BOOST_TEST(!d.check_add_edge(e2).has_value());
  BOOST_TEST(partial_equal(d.topo_order, {0, 1, 2, 3, 4}));

  auto e3 = add_edge(4, 2, *g).first;
  BOOST_TEST(cycle_equal(d.check_add_edge(e3).value(), {2, 3, 4}, *g));
}

BOOST_AUTO_TEST_CASE(minimal_cycle) {
  auto [g, d] = create_graph(5, {{0, 1}, {0, 2}, {1, 4}, {2, 3}, {3, 4}});

  auto e = add_edge(4, 0, *g).first;
  BOOST_TEST(cycle_equal(d.check_add_edge(e).value(), {0, 1, 4}, *g));
}
