#ifndef INCLUDE_TRIPENGINE_H_
#define INCLUDE_TRIPENGINE_H_
#include <iomanip>
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#include <random>
#include <sstream>
#include <string>
#include "csv.h"

#include "MapEngine.h"
#include "Trip.h"
#include "utils.h"

using csv_stream = io::CSVReader<5, io::trim_chars<' ', '\t'>, io::double_quote_escape<',', '"'>>;
using ChicagoTaxiTrip = std::tuple<uint32_t, uint32_t, int64_t, int64_t>;


struct OGRGeometryDeleter {
  void operator()(OGRGeometry *p) const { OGRGeometryFactory::destroyGeometry(p); }
};
struct GDALDatasetDeleter {
  void operator()(GDALDataset *p) const { GDALClose(p); }
};
struct ChicagoTaxiTripEngine {
  csv_stream inp;
  uint32_t max_row_count;
  std::vector<std::tuple<int64_t, std::unique_ptr<OGRGeometry, OGRGeometryDeleter>, OGREnvelope>> census_tracts, community_areas;
  std::vector<ChicagoTaxiTrip> cached;
  std::stringstream ss;
  std::tm temp{}, current{};
  uint32_t increment_id = 0;
  std::chrono::system_clock::time_point zero = std::chrono::system_clock::from_time_t(0);
  ChicagoTaxiTripEngine(const std::string &csv_file,
                        const std::string &ca_file,
                        const std::string &ct_file,
                        size_t max_row_count = std::numeric_limits<uint32_t>::max());

  // TODO: refactor with coroutine
  std::optional<std::pair<std::tm, std::vector<ChicagoTaxiTrip>>> next();

  /**
   * this function should be called inside multithreading regions (i.e. chop the data into smaller pieces and call this function)
   * @param data chicago taxi trip collections with pickup/dropoff region
   * @param me map engine, necessary for the shape of pickup/dropoff region
   * @param ch_query the query object (costly to construct, can be recycled)
   * @param lookup_radius the radius to search the nearest node
   * @param max_iter the number of tries to generate pair of locations that are both close to some node in the map and reachable from each other
   */
  std::vector<Trip> generate_data(std::vector<ChicagoTaxiTrip> &data,
                                  const MapEngine &me,
                                  RoutingKit::ContractionHierarchyQuery &ch_query,
                                  float lookup_radius = 50,
                                  size_t max_iter = 10);
};

#endif // INCLUDE_TRIPENGINE_H_
