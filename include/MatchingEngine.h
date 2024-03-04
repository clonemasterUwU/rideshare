#ifndef MODEL_GEN_MK2_INCLUDE_MATCHINGENGINE_H_
#define MODEL_GEN_MK2_INCLUDE_MATCHINGENGINE_H_
#include <iostream>
#include <span>
#include "MapEngine.h"
#include "MaximumWeightedMatching.h"
#include "TripEngine.h"
using MWM = MaximumWeightedMatching<int64_t, int64_t>;
using Edge = MWM::InputEdge;
enum Match {
  NO_MATCH,
  ABBA,
  ABAB,
  BABA,
  BAAB,
};
constexpr double delta = 0.1;
struct MatchingEngine {
    static void offline_optimal_matching(std::span<Trip> window,
                                         const RoutingKit::SimpleOSMCarRoutingGraph &graph,
                                         RoutingKit::ContractionHierarchyQuery &query,
                                         std::ostream &out) {
      size_t n = window.size();

      std::vector<Edge> edges;
      std::vector<unsigned> nodes;
      std::vector<std::vector<unsigned>> dist;
      std::vector<std::vector<Match>> types(n, std::vector<Match>(n, NO_MATCH));
      nodes.reserve(2 * n);
      for (const Trip &trip : window) {
        nodes.push_back(trip.begin_node);
        nodes.push_back(trip.end_node);
      }
      dist.reserve(2 * n);
      query.reset().pin_targets(nodes);
      for (auto node : nodes) {
        dist.push_back(query.reset_source().add_source(node).run_to_pinned_targets().get_distances_to_targets());
      }
      for (int i = 0; i < n; i++) {
        auto sa = i << 1, ea = i << 1 | 1;
        int64_t base_a = dist[sa][ea];
        ASSERT(base_a == window[i].len);
        int64_t saved = 0;
        for (int j = i + 1; j < n; j++) {
          auto sb = j << 1, eb = j << 1 | 1;
          int64_t base_b = dist[sb][eb];
          Match type{NO_MATCH};
          // abab
          {
            int64_t sum = dist[sa][sb] + dist[sb][ea] + dist[ea][eb], detour_a = dist[sa][sb] + dist[sb][ea] - base_a,
                    detour_b = dist[sb][ea] + dist[ea][eb] - base_b;
            if (detour_a < delta * base_a && detour_b < delta * base_b && sum - base_a - base_b > saved) {
              saved = sum - base_a - base_b;
              type = ABAB;
            }
          }
          // abba
          {
            int64_t sum = dist[sa][sb] + dist[sb][eb] + dist[eb][ea], detour_a = sum - base_a;
            if (detour_a < delta * base_a && sum - base_a - base_b > saved) {
              saved = sum - base_a - base_b;
              type = ABBA;
            }
          }
          // baab
          {
            int64_t sum = dist[sb][sa] + dist[sa][ea] + dist[ea][eb], detour_b = sum - base_b;
            if (detour_b < delta * base_b && sum - base_a - base_b > saved) {
              saved = sum - base_a - base_b;
              type = BAAB;
            }
          }
          // baba
          {
            int64_t sum = dist[sb][sa] + dist[sa][eb] + dist[eb][ea], detour_a = dist[sa][eb] + dist[eb][ea] - base_a,
                    detour_b = dist[sb][sa] + dist[sa][eb] - base_b;
            if (detour_a < delta * base_a && detour_b < delta * base_b && sum - base_a - base_b > saved) {
              saved = sum - base_a - base_b;
              type = BABA;
            }
          }
          if (type != NO_MATCH) {
            types[i][j] = type;
            edges.push_back({i + 1, j + 1, saved});
          }
        }
      }

      MWM optimizer(n, edges);
      std::vector<std::pair<int, int>> matches;
      auto max_saved = optimizer.maximum_weighted_matching(matches);
      out << n << " " << std::accumulate(window.begin(), window.end(), 0ULL, [](auto a, const auto &b) { return a + b.len; }) << " " << max_saved << "\n";
      // for (const Trip &trip : window) {
      //   out << trip.begin_x << " " << trip.begin_y << " " << trip.end_x << " " << trip.end_y << "\n";
      // }
      // for (auto &[a, b] : matches) {
      //   if (a > b)
      //     std::swap(a, b);
      // }
      // std::sort(matches.begin(), matches.end());
      // out << edges.size() << "\n";
      // {
      //   size_t p = 0;
      //   for (auto [i, j, v] : edges) {
      //     out << (i - 1) << " " << (j - 1) << " " << v << " ";
      //     if (p < matches.size() && matches[p].first == i && matches[p].second == j) {
      //       out << 1 << "\n";
      //       p++;
      //     } else {
      //       out << 0 << "\n";
      //     }
      //   }
      //   ASSERT(p == matches.size());
      // }
      // out << max_saved << " " << matches.size() << "\n";
      // for (auto [x, y] : matches) {
      //   --x, --y;
      //   ASSERT(types[x][y] != NO_MATCH);
      //   out << x << " " << y << "\n";
      //   query.reset().add_source(window[x].begin_node).add_target(window[x].end_node).run();
      //   for (auto node : query.get_node_path()) {
      //     out << graph.longitude[node] << " " << graph.latitude[node] << " ";
      //   }
      //   out << "\n";
      //   query.reset().add_source(window[y].begin_node).add_target(window[y].end_node).run();
      //   for (auto node : query.get_node_path()) {
      //     out << graph.longitude[node] << " " << graph.latitude[node] << " ";
      //   }
      //   out << "\n";
      //   std::array<unsigned, 4> order;
      //   if (types[x][y] == ABBA) {
      //     order = {{window[x].begin_node, window[y].begin_node, window[y].end_node, window[x].end_node}};
      //   } else if (types[x][y] == ABAB) {
      //     order = {{window[x].begin_node, window[y].begin_node, window[x].end_node, window[y].end_node}};
      //   } else if (types[x][y] == BABA) {
      //     order = {{window[y].begin_node, window[x].begin_node, window[y].end_node, window[x].end_node}};
      //   } else {
      //     // baab
      //     order = {{window[y].begin_node, window[x].begin_node, window[x].end_node, window[y].end_node}};
      //   }
      //   for (int i = 0; i + 1 < 4; i++) {
      //     query.reset().add_source(order[i]).add_target(order[i + 1]).run();
      //     for (auto node : query.get_node_path()) {
      //       out << graph.longitude[node] << " " << graph.latitude[node] << " ";
      //     }
      //   }
      //   out << "\n";
      // }
      // out.flush();
    }
};
#endif // MODEL_GEN_MK2_INCLUDE_MATCHINGENGINE_H_
