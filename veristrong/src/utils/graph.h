#ifndef CHECKER_UTILS_GRAPH_H
#define CHECKER_UTILS_GRAPH_H

#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <ranges>
#include <syncstream>
#include <unordered_map>
#include <utility>

#include "boost/container_hash/extensions.hpp"

namespace checker::utils {
template <typename T>
concept HashAndEquality = requires(T t) {
  { std::hash<T>{}(t) } -> std::same_as<size_t>;
  { std::equal_to<T>{}(t, t) } -> std::same_as<bool>;
};

template <HashAndEquality Vertex, typename Edge>
struct Graph {
  using InternalGraph =
      boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS>;

  std::unique_ptr<InternalGraph> graph = std::make_unique<InternalGraph>();
  boost::bimap<
      boost::bimaps::unordered_set_of<Vertex, std::hash<Vertex>>,
      boost::bimaps::unordered_set_of<InternalGraph::vertex_descriptor>>
      vertex_map;
  std::unordered_map<InternalGraph::edge_descriptor, Edge,
                     boost::hash<InternalGraph::edge_descriptor>>
      edge_map;

  auto add_vertex(Vertex v) -> const Vertex & {
    return vertex_map.insert({v, boost::add_vertex(*graph)})
        .first->get_left_pair()
        .first;
  }

  auto vertex(Vertex v) const
      -> std::optional<std::reference_wrapper<const Vertex>> {
    if (auto it = vertex_map.left.find(v); it != vertex_map.left.end()) {
      return std::ref(it->first);
    } else {
      return std::nullopt;
    }
  }

  auto add_edge(Vertex from, Vertex to, Edge e) -> const Edge & {
    auto from_desc = vertex_map.left.at(from);
    auto to_desc = vertex_map.left.at(to);

    auto [desc, _] = boost::add_edge(from_desc, to_desc, *graph);
    return edge_map.try_emplace(desc, e).first->second;
  }

  auto edge(Vertex from, Vertex to)
      -> std::optional<std::reference_wrapper<Edge>> {
    if (auto r = std::as_const(*this).edge(from, to); r) {
      return std::ref(const_cast<Edge &>(r.value().get()));
    } else {
      return std::nullopt;
    }
  }

  auto edge(Vertex from, Vertex to) const
      -> std::optional<std::reference_wrapper<const Edge>> {
    auto from_desc = vertex_map.left.at(from);
    auto to_desc = vertex_map.left.at(to);

    if (auto [desc, has_edge] = boost::edge(from_desc, to_desc, *graph);
        has_edge) {
      return std::ref(edge_map.at(desc));
    } else {
      return std::nullopt;
    }
  }

  auto successors(Vertex vertex) const -> std::ranges::range auto{
    auto [begin, end] = boost::out_edges(vertex_map.left.at(vertex), *graph);
    return std::ranges::subrange(begin, end)  //
           | std::ranges::views::transform([&](auto desc) -> decltype(auto) {
               return vertex_map.right.at(boost::target(desc, *graph));
             });
  }

  auto vertices() const -> std::ranges::range auto{
    auto [begin, end] = boost::vertices(*graph);
    return std::ranges::subrange(begin, end)  //
           | std::ranges::views::transform([&](auto desc) -> decltype(auto) {
               return vertex_map.right.at(desc);
             });
  }

  auto num_vertices() const -> ssize_t { return boost::num_vertices(*graph); }

  auto num_edges() const -> ssize_t { return boost::num_edges(*graph); }

  auto edges() const -> std::ranges::range auto{
    auto [begin, end] = boost::edges(*graph);
    return std::ranges::subrange(begin, end)  //
           | std::ranges::views::transform([&](auto desc) -> decltype(auto) {
               return std::tuple{
                   std::ref(vertex_map.right.at(boost::source(desc, *graph))),
                   std::ref(vertex_map.right.at(boost::target(desc, *graph))),
                   std::ref(edge_map.at(desc)),
               };
             });
  }

  friend auto operator<<(std::ostream &os, const Graph &graph)
      -> std::ostream & {
    auto out = std::osyncstream{os};

    for (const auto &[from, to, e] : graph.edges()) {
      out << from << "->" << to << ' ' << e << '\n';
    }

    return os;
  }
};
}  // namespace checker::utils

#endif  // CHECKER_UTILS_GRAPH_H
