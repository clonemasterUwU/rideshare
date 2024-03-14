#include "MapEngine.h"
MapEngine::MapEngine(const std::string &osm_file) {
    graph = RoutingKit::simple_load_osm_car_routing_graph_from_pbf(osm_file);
//#pragma omp task default(none) shared(ch_time, graph)
    ch_time =
        RoutingKit::ContractionHierarchy::build(graph.node_count(), RoutingKit::invert_inverse_vector(graph.first_out), graph.head, graph.travel_time);
// #pragma omp task default(none) shared(ch_dist, graph)
//       ch_dist = RoutingKit::ContractionHierarchy::build(graph.node_count(), RoutingKit::invert_inverse_vector(graph.first_out), graph.head,
//       graph.geo_distance);
//#pragma omp task default(none) shared(map_geo_position, graph)
    map_geo_position = RoutingKit::GeoPositionToNode(graph.latitude, graph.longitude);
}


