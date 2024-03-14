#include "MatchingEngine.h"

std::vector<std::tuple<uint32_t, uint32_t, unsigned, Match>> MatchingEngine::offline_optimal_matching(const std::span<Trip> window,
                                                                                                      const MapEngine &map_engine,
                                                                                                      RoutingKit::ContractionHierarchyQuery &time_query,
                                                                                                      float delta) {
    size_t n = window.size();
    // log before create graph
//      if (log_message) {
//          log_message(std::to_string(n));
//          for (const Trip &trip : window) {
//              log_message(
//                  std::to_string(trip.begin_x) + " " + std::to_string(trip.begin_y) + " " + std::to_string(trip.end_x) + " " + std::to_string(trip.end_y));
//          }
//      }
    std::vector<Edge> edges;
    std::vector<unsigned> nodes;
    std::vector<std::vector<unsigned>> dist;
    std::vector<std::vector<Match>> types(n, std::vector<Match>(n, Match::NO_MATCH));
    nodes.reserve(2 * n);
    for (const Trip &trip : window) {
        nodes.push_back(trip.begin_node);
        nodes.push_back(trip.end_node);
    }
    dist.reserve(2 * n);
    time_query.reset().pin_targets(nodes);
    for (auto node : nodes) {
        dist.push_back(time_query.reset_source().add_source(node).run_to_pinned_targets().get_distances_to_targets());
    }
    for (int i = 0; i < n; i++) {
        auto sa = i << 1, ea = i << 1 | 1;
        int64_t base_a = dist[sa][ea];
        int64_t saved = 0;
        for (int j = i + 1; j < n; j++) {
            auto sb = j << 1, eb = j << 1 | 1;
            int64_t base_b = dist[sb][eb];
            Match type{Match::NO_MATCH};
            // abab
            {
                int64_t sum = dist[sa][sb] + dist[sb][ea] + dist[ea][eb], detour_a = dist[sa][sb] + dist[sb][ea] - base_a,
                    detour_b = dist[sb][ea] + dist[ea][eb] - base_b;
                if (detour_a < delta * base_a && detour_b < delta * base_b && sum - base_a - base_b > saved) {
                    saved = sum - base_a - base_b;
                    type = Match::ABAB;
                }
            }
            // abba
            {
                int64_t sum = dist[sa][sb] + dist[sb][eb] + dist[eb][ea], detour_a = sum - base_a;
                if (detour_a < delta * base_a && sum - base_a - base_b > saved) {
                    saved = sum - base_a - base_b;
                    type = Match::ABBA;
                }
            }
            // baab
            {
                int64_t sum = dist[sb][sa] + dist[sa][ea] + dist[ea][eb], detour_b = sum - base_b;
                if (detour_b < delta * base_b && sum - base_a - base_b > saved) {
                    saved = sum - base_a - base_b;
                    type = Match::BAAB;
                }
            }
            // baba
            {
                int64_t sum = dist[sb][sa] + dist[sa][eb] + dist[eb][ea], detour_a = dist[sa][eb] + dist[eb][ea] - base_a,
                    detour_b = dist[sb][sa] + dist[sa][eb] - base_b;
                if (detour_a < delta * base_a && detour_b < delta * base_b && sum - base_a - base_b > saved) {
                    saved = sum - base_a - base_b;
                    type = Match::BABA;
                }
            }
            if (type != Match::NO_MATCH) {
                types[i][j] = type;
                edges.push_back({i + 1, j + 1, saved});
            }
        }
    }

    MWM optimizer(n, edges);
    std::vector<std::pair<int, int>> matches;
    auto max_saved = optimizer.maximum_weighted_matching(matches);
//      if (log_message) {
//          log_message(std::to_string(edges.size()) + " " + std::to_string(matches.size()) + " " + std::to_string(max_saved) + " " +
//              std::to_string(std::accumulate(window.begin(), window.end(), 0ULL, [](auto a, const Trip &b) { return a + b.time_len; })));
//          size_t p = 0;
//          for (auto [i, j, w] : edges) {
//              if (p < matches.size() && matches[p].first == i && matches[p].second == j) {
//                  ASSERT(types[i - 1][j - 1] != NO_MATCH);
//                  p++;
//              }
//              --i, --j;
//              log_message(std::to_string(i) + " " + std::to_string(j) + " " + std::to_string(w) + " " + std::to_string(types[i][j]));
//          }
//          ASSERT(p == matches.size());
//      }
    std::vector<std::tuple<uint32_t, uint32_t, unsigned, Match>> results;
    results.reserve(edges.size());
    for (auto [i, j, w] : edges) {
        --i, --j;
        results.emplace_back(window[i].id, window[j].id, w, types[i][j]);
    }
    return {results};
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
