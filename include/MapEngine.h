#ifndef INCLUDE_MAPENGINE_H_
#define INCLUDE_MAPENGINE_H_
#include <string>
#include "routingkit/contraction_hierarchy.h"
#include "routingkit/geo_position_to_node.h"
#include "routingkit/inverse_vector.h"
#include "routingkit/osm_simple.h"
#include "utils.h"

struct MapEngine {
  private:
    RoutingKit::SimpleOSMCarRoutingGraph graph;
    RoutingKit::ContractionHierarchy ch_time;
    // RoutingKit::ContractionHierarchy ch_dist;
    RoutingKit::GeoPositionToNode map_geo_position;

  public:
    explicit MapEngine(const std::string &osm_file);
    [[nodiscard]] inline node_t find_nearest_neighbor_within_radius(float query_latitude, float query_longitude, float query_radius) const {
        return map_geo_position.find_nearest_neighbor_within_radius(query_latitude, query_longitude, query_radius).id;
    };
    [[nodiscard]] inline std::pair<float, float> get_lat_lon_from_node(node_t node) const { return std::make_pair(graph.latitude[node], graph.longitude[node]); }
    [[nodiscard]] inline RoutingKit::ContractionHierarchyQuery get_time_query_object() const { return RoutingKit::ContractionHierarchyQuery(ch_time); };
};

#endif // INCLUDE_MAPENGINE_H_
