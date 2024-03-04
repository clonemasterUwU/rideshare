#ifndef MODEL_GEN_MK2_INCLUDE_MAPENGINE_H_
#define MODEL_GEN_MK2_INCLUDE_MAPENGINE_H_
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#include <omp.h>
#include <routingkit/contraction_hierarchy.h>
#include <routingkit/geo_position_to_node.h>
#include <routingkit/inverse_vector.h>
#include <routingkit/osm_simple.h>
#include "utils.h"
struct OGRGeometryDeleter {
    void operator()(OGRGeometry *p) const { OGRGeometryFactory::destroyGeometry(p); }
};
struct GDALDatasetDeleter {
    void operator()(GDALDataset *p) const { GDALClose(p); }
};
struct MapEngine {
    std::vector<std::tuple<int64_t, std::unique_ptr<OGRGeometry, OGRGeometryDeleter>, OGREnvelope>> census_tracts, community_areas;
    RoutingKit::ContractionHierarchy ch;
    RoutingKit::SimpleOSMCarRoutingGraph graph;
    RoutingKit::GeoPositionToNode map_geo_position;
    MapEngine(const std::string &census_file, const std::string &community_area_file, const std::string &osm_file) {
#pragma omp parallel default(shared)
      {
#pragma omp single
        {
#pragma omp task
          {
            OGRRegisterAll();
            auto census_shapes = std::unique_ptr<GDALDataset, GDALDatasetDeleter>(
                     (GDALDataset *) GDALOpenEx(census_file.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)),
                 ca_shapes = std::unique_ptr<GDALDataset, GDALDatasetDeleter>(
                     (GDALDataset *) GDALOpenEx(community_area_file.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
            OGRLayer *layer = census_shapes->GetLayer(0);
            layer->ResetReading();
            OGRFeature *feature;
            while ((feature = layer->GetNextFeature()) != nullptr) {
              OGRGeometry *geometry = feature->StealGeometry();
              ASSERT(geometry != nullptr);
              auto id = feature->GetFieldAsInteger64("geoid10");
              ASSERT(id != 0);
              census_tracts.emplace_back(id, std::unique_ptr<OGRGeometry, OGRGeometryDeleter>(geometry), OGREnvelope());
              OGRFeature::DestroyFeature(feature);
            }
            std::sort(census_tracts.begin(), census_tracts.end(), [](const auto &a, const auto &b) { return std::get<0>(a) < std::get<0>(b); });
            for (auto &[_, region, envelope] : census_tracts)
              region->getEnvelope(&envelope);
            layer = ca_shapes->GetLayer(0);
            layer->ResetReading();
            while ((feature = layer->GetNextFeature()) != nullptr) {
              OGRGeometry *geometry = feature->StealGeometry();
              ASSERT(geometry != nullptr);
              auto id = feature->GetFieldAsInteger64("area_num_1");
              ASSERT(id != 0);
              community_areas.emplace_back(id, std::unique_ptr<OGRGeometry, OGRGeometryDeleter>(geometry), OGREnvelope());
              OGRFeature::DestroyFeature(feature);
            }
            std::sort(community_areas.begin(), community_areas.end(), [](const auto &a, const auto &b) { return std::get<0>(a) < std::get<0>(b); });

            for (auto &[_, region, envelope] : community_areas)
              region->getEnvelope(&envelope);
          }
#pragma omp task shared(graph, ch, map_geo_position, osm_file) default(none)
          {
            // TODO: add logging
            graph = RoutingKit::simple_load_osm_car_routing_graph_from_pbf(osm_file);
#pragma omp task shared(ch, graph) default(none)
            {
              // Build the shortest path index
              ch = RoutingKit::ContractionHierarchy::build(
                  graph.node_count(), RoutingKit::invert_inverse_vector(graph.first_out), graph.head, graph.travel_time);
            }
#pragma omp task shared(graph, map_geo_position) default(none)
            {
              // Build the index to quickly map latitudes and longitudes
              map_geo_position = RoutingKit::GeoPositionToNode(graph.latitude, graph.longitude);
            }
          }
        }
      }
    }
};
#endif // MODEL_GEN_MK2_INCLUDE_MAPENGINE_H_
