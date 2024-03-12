#ifndef INCLUDE_TRIPENGINE_H_
#define INCLUDE_TRIPENGINE_H_
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <optional>
#include "MapEngine.h"
#include "csv.h"
#include "utils.h"

struct OGRGeometryDeleter {
    void operator()(OGRGeometry *p) const { OGRGeometryFactory::destroyGeometry(p); }
};
struct GDALDatasetDeleter {
    void operator()(GDALDataset *p) const { GDALClose(p); }
};
using csv_stream = io::CSVReader<5, io::trim_chars<' ', '\t'>, io::double_quote_escape<',', '"'>>;
constexpr unsigned max_dist = 50;
struct ChicagoTaxiTrip {
    csv_stream inp;
    std::vector<std::tuple<int64_t, std::unique_ptr<OGRGeometry, OGRGeometryDeleter>, OGREnvelope>> census_tracts, community_areas;
    ChicagoTaxiTrip(const std::string &csv_file) : inp(csv_file) {
      inp.read_header(
          io::ignore_extra_column, "Trip Start Timestamp", "Pickup Census Tract", "Dropoff Census Tract", "Pickup Community Area", "Dropoff Community Area");
    }

    /**
     * this function should be called inside multithreaded regions (i.e. chop the data into smaller pieces and call this function)
     * @param data trip collections with pickup/dropoff region
     * @param me map engine, necessary for the shape of pickup/dropoff region
     * @param ch_query the query object (costly to construct, can be recycled)
     * @param max_iter the number of tries to generate pair of locations that are both close to some node in the map and reachable from each other
     */
    static void generate_data(std::vector<Trip> &data, const MapEngine &me, RoutingKit::ContractionHierarchyQuery &ch_query, size_t max_iter = 10) {
      std::mt19937 rd(8102001);
      std::cout << data.size() << "\n";
      //remove data with no pickup/dropoff region
      auto remove_ptr = std::remove_if(
          //      std::execution::par_unseq,
          data.begin(),
          data.end(),
          [](const Trip &trip) { return trip.begin_census_or_ca == 0 || trip.end_census_or_ca == 0; });
      data.erase(remove_ptr, data.end());
      std::cout << data.size() << "\n";
      for (Trip &trip : data) {
        trip.len = RoutingKit::inf_weight;
        auto geo_mapping = [&me](int64_t i) -> std::optional<std::pair<const OGRGeometry *, const OGREnvelope *>> {
          ASSERT(i != 0);
          if (i > 0) {
            auto pos = std::lower_bound(me.census_tracts.begin(), me.census_tracts.end(), i, [](const auto &p, int64_t x) { return std::get<0>(p) < x; }) -
                me.census_tracts.begin();
            if (pos < me.census_tracts.size() && std::get<0>(me.census_tracts[pos]) == i)
              return std::make_optional(std::make_pair(std::get<1>(me.census_tracts[pos]).get(), &std::get<2>(me.census_tracts[pos])));
            else {
              return std::nullopt;
            }
          } else {
            i = -i;
            auto pos = std::lower_bound(me.community_areas.begin(), me.community_areas.end(), i, [](const auto &p, int64_t x) { return std::get<0>(p) < x; }) -
                me.community_areas.begin();
            if (pos < me.community_areas.size() && std::get<0>(me.community_areas[pos]) == i)
              return std::make_optional(std::make_pair(std::get<1>(me.community_areas[pos]).get(), &std::get<2>(me.community_areas[pos])));
            else {
              return std::nullopt;
            }
          }
        };
        auto begin_pair = geo_mapping(trip.begin_census_or_ca);
        auto end_pair = geo_mapping(trip.end_census_or_ca);
        if (!begin_pair || !end_pair)
          continue;
        auto [begin, begin_bound] = *begin_pair;
        auto [end, end_bound] = *end_pair;
        std::uniform_real_distribution<float> begin_x_rand(begin_bound->MinX, begin_bound->MaxX), begin_y_rand(begin_bound->MinY, begin_bound->MaxY),
            end_x_rand(end_bound->MinX, end_bound->MaxX), end_y_rand(end_bound->MinY, end_bound->MaxY);
        for (size_t iter = 0; iter < max_iter; iter++) {
          float begin_x = begin_x_rand(rd), end_x = end_x_rand(rd), begin_y = begin_y_rand(rd), end_y = end_y_rand(rd);
          unsigned from = me.map_geo_position.find_nearest_neighbor_within_radius(begin_y, begin_x,max_dist ).id;
          if (from == RoutingKit::invalid_id) {
            continue;
          }
          unsigned to = me.map_geo_position.find_nearest_neighbor_within_radius(end_y, end_x, max_dist).id;
          if (to == RoutingKit::invalid_id) {
            continue;
          }
          ch_query.reset().add_source(from).add_target(to).run();
          auto distance = ch_query.get_distance();
          if (distance == RoutingKit::inf_weight)
            continue;
          trip.begin_x = begin_x;
          trip.end_x = end_x;
          trip.begin_y = begin_y;
          trip.end_y = end_y;
          trip.begin_node = from;
          trip.end_node = to;
          trip.len = distance;
          break;
        }
      }

      remove_ptr = std::remove_if(
          //      std::execution::par_unseq,
          data.begin(),
          data.end(),
          [](const Trip &trip) { return trip.len == RoutingKit::inf_weight; });
      data.erase(remove_ptr, data.end());
      std::cout << data.size() << "\n";
      std::sort(data.begin(), data.end(), [](const Trip &a, const Trip &b) { return a.tick < b.tick; });
    }
};
#endif // INCLUDE_TRIPENGINE_H_
