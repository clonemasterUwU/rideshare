#ifndef MATCHINGENGINE_H_
#define MATCHINGENGINE_H_
#include <span>
#include <tuple>
#include <numeric>
#include <glaze/glaze.hpp>
#include "MapEngine.h"
#include "MaximumWeightedMatching.h"
#include "Trip.h"
using MWM = MaximumWeightedMatching<int64_t, int64_t>;
using Edge = MWM::InputEdge;
enum class Match {
  NO_MATCH = 0,
  ABBA = 1,
  ABAB = 2,
  BABA = 3,
  BAAB = 4,
};
template<>
struct glz::meta<Match> {
  using
  enum Match;
  static constexpr auto value = enumerate(NO_MATCH, ABBA, ABAB, BABA, BAAB);
};
struct MatchResult {
  uint32_t tick;
  std::vector<std::tuple<uint32_t, uint32_t, unsigned, Match>> matches;
};
using path = std::vector<float>;
struct PathExtension {
  std::vector<std::pair<uint32_t, path>> base_path;
  std::vector<std::pair<uint32_t, path>> new_path;
};
struct MatchingEngine {

  static
  std::vector<std::tuple<uint32_t, uint32_t, unsigned, Match>> offline_optimal_matching(const std::span<Trip> window,
                                                                                        const MapEngine &map_engine,
                                                                                        RoutingKit::ContractionHierarchyQuery &time_query,
//                           RoutingKit::ContractionHierarchyQuery &dist_query,
                                                                                        float delta);
};
#endif // MATCHINGENGINE_H_
