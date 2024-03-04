#include <omp.h>
#include "MapEngine.h"
#include "MatchingEngine.h"
#include "TripEngine.h"
int main() {
#pragma omp parallel default(none)
  {
#pragma omp single
    {
      std::unique_ptr<MapEngine> me{};
      std::unique_ptr<TripEngine> te{};
#pragma omp task shared(me) default(none)
      {
        me = std::make_unique<MapEngine>("../data/census_tract_2010/geo_export_e2913d18-a7cf-44d8-b59d-b72924c4cf29.shp",
                                         "../data/community_area/geo_export_b57457bd-82f6-4c7c-beb0-3225ef658b15.shp",
                                         "../data/Chicago.osm.pbf");
      }
#pragma omp task shared(te) default(none)
      { te = std::make_unique<TripEngine>("../data/taxi_data.csv"); }
#pragma omp taskwait

      // read full day
      size_t max_row_count = 1000000
          // std::numeric_limits<size_t>::max()
          ;
      std::tm temp{}, current{};
      std::stringstream ss;
      std::vector<Trip> data;
      auto zero = std::chrono::system_clock::from_time_t(0);
      char *trip_start_timestamp, *pickup_census_tract, *dropoff_census_tract, *pickup_community_area, *dropoff_community_area;
      while (max_row_count-- &&
             te->inp.read_row(trip_start_timestamp, pickup_census_tract, dropoff_census_tract, pickup_community_area, dropoff_community_area)) {
        ss << trip_start_timestamp << " ";
        ss >> std::get_time(&temp, "%m/%d/%Y %I:%M:%S %p");
        if (current.tm_mday != temp.tm_mday) {
          if (current.tm_year != 0) {
            // we gather a day full data, spin up a thread to do the computation
#pragma omp task shared(me) firstprivate(data, current) default(none)
            {
              RoutingKit::ContractionHierarchyQuery ch_query(me->ch);
              TripEngine::generate_data(data, *me, ch_query);
              ASSERT(!data.empty());
              std::stringstream out;
              out << "../data/" << std::put_time(&current, "%Y_%m_%d") << ".txt";
              std::ofstream out_file(out.str());
              size_t i = 0, n = data.size();
              // with every window of the same tick do the oracle matching
              while (i < n) {
                size_t j = i + 1;
                while (j < n && data[j].tick == data[i].tick)
                  j++;
                MatchingEngine::offline_optimal_matching(std::span<Trip>(data.begin() + i, data.begin() + j), me->graph, ch_query, out_file);
                i = j;
              }
            }
          }
          current = temp;
          data.clear();
        }
        int64_t begin, end;
        if (strcmp(pickup_census_tract, "") != 0) {
          begin = std::atoll(pickup_census_tract);
        } else if (strcmp(pickup_community_area, "") != 0) {
          begin = -std::atoll(pickup_community_area);
        } else {
          begin = 0;
        }
        if (strcmp(dropoff_census_tract, "") != 0) {
          end = std::atoll(dropoff_census_tract);
        } else if (strcmp(dropoff_community_area, "") != 0) {
          end = -std::atoll(dropoff_community_area);
        } else {
          end = 0;
        }
        auto tick = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::from_time_t(std::mktime(&temp)) - zero).count();
        data.emplace_back(tick, begin, end);
      }
// wait here otherwise our pointers will dangle, tasks need them to compute
#pragma omp taskwait
    }
  }
}
