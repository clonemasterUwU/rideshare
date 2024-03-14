#include "ChicagoTaxiTripEngine.h"

ChicagoTaxiTripEngine::ChicagoTaxiTripEngine(const std::string &csv_file, const std::string &ca_file, const std::string &ct_file, size_t max_row_count) :
    inp(csv_file), max_row_count(max_row_count) {
    inp.read_header(
        io::ignore_extra_column, "Trip Start Timestamp", "Pickup Census Tract", "Dropoff Census Tract", "Pickup Community Area", "Dropoff Community Area");
    auto ct_shapes = std::unique_ptr<GDALDataset, GDALDatasetDeleter>((GDALDataset *) GDALOpenEx(ct_file.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)),
        ca_shapes = std::unique_ptr<GDALDataset, GDALDatasetDeleter>((GDALDataset *) GDALOpenEx(ca_file.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
//      if (log_message) {
//        log_message("ct layers count: " + std::to_string(ct_shapes->GetLayerCount()) + ", ca layers count: " + std::to_string(ca_shapes->GetLayerCount()));
//      }
    OGRLayer *layer = ct_shapes->GetLayer(0);
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

std::optional<std::pair<std::tm, std::vector<ChicagoTaxiTrip>>> ChicagoTaxiTripEngine::next() {
    char *trip_start_timestamp, *pickup_census_tract, *dropoff_census_tract, *pickup_community_area, *dropoff_community_area;
    while (increment_id < max_row_count &&
        inp.read_row(trip_start_timestamp, pickup_census_tract, dropoff_census_tract, pickup_community_area, dropoff_community_area)) {
        auto parse = [&]() -> ChicagoTaxiTrip {
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
          return {increment_id, tick, begin, end};
        };
        ss << trip_start_timestamp << " ";
        ss >> std::get_time(&temp, "%m/%d/%Y %I:%M:%S %p");
        if (current.tm_mday != temp.tm_mday) {
            if (current.tm_year != 0) {
                // we gather a day full data, spin up a thread to do the computation
                {
                    ASSERT(!cached.empty());
                    std::vector<ChicagoTaxiTrip> result;
                    result.push_back(parse());
                    auto temp_cur = std::exchange(current,temp);
                    increment_id++;
                    std::swap(result, cached);
                    return {{temp_cur, std::move(result)}};
                }
            }
            current = temp;
        }
        cached.push_back(parse());
        increment_id++;
    }
    if (cached.empty())
        return std::nullopt;
    std::vector<ChicagoTaxiTrip> result;
    std::swap(result, cached);
    return {{current, std::move(result)}};
}

std::vector<Trip> ChicagoTaxiTripEngine::generate_data(std::vector<ChicagoTaxiTrip> &data,
                                                       const MapEngine &me,
                                                       RoutingKit::ContractionHierarchyQuery &ch_query,
                                                       float lookup_radius,
                                                       size_t max_iter) {
    std::mt19937 rd(8102001);
    //      std::cout << data.size() << "\n";
    // remove data with no pickup/dropoff region
    auto remove_ptr = std::remove_if(
        //      std::execution::par_unseq,
        data.begin(),
        data.end(),
        [](const ChicagoTaxiTrip &trip) { return std::get<2>(trip) == 0 || std::get<3>(trip) == 0; });
    data.erase(remove_ptr, data.end());
    //      std::cout << data.size() << "\n";
    std::vector<Trip> result;
    result.reserve(data.size());
    for (auto [id, tick, begin_id, end_id] : data) {
        auto geo_mapping = [&ct = census_tracts, &ca = community_areas](int64_t i) -> std::optional<std::pair<const OGRGeometry *, const OGREnvelope *>> {
          ASSERT(i != 0);
          if (i > 0) {
              auto pos = std::lower_bound(ct.begin(), ct.end(), i, [](const auto &p, int64_t x) { return std::get<0>(p) < x; }) - ct.begin();
              if (pos < ct.size() && std::get<0>(ct[pos]) == i)
                  return {{std::get<1>(ct[pos]).get(), &std::get<2>(ct[pos])}};
              else {
                  return std::nullopt;
              }
          } else {
              i = -i;
              auto pos = std::lower_bound(ca.begin(), ca.end(), i, [](const auto &p, int64_t x) { return std::get<0>(p) < x; }) - ca.begin();
              if (pos < ca.size() && std::get<0>(ca[pos]) == i)
                  return {{std::get<1>(ca[pos]).get(), &std::get<2>(ca[pos])}};
              else {
                  return std::nullopt;
              }
          }
        };
        auto begin_pair = geo_mapping(begin_id);
        auto end_pair = geo_mapping(end_id);
        if (!begin_pair || !end_pair)
            continue;
        auto [begin, begin_bound] = *begin_pair;
        auto [end, end_bound] = *end_pair;
        std::uniform_real_distribution<float> begin_x_rand(begin_bound->MinX, begin_bound->MaxX), end_x_rand(end_bound->MinX, end_bound->MaxX),
            begin_y_rand(begin_bound->MinY, begin_bound->MaxY), end_y_rand(end_bound->MinY, end_bound->MaxY);
        for (size_t iter = 0; iter < max_iter; iter++) {
            float begin_x = begin_x_rand(rd), end_x = end_x_rand(rd), begin_y = begin_y_rand(rd), end_y = end_y_rand(rd);
            unsigned from = me.find_nearest_neighbor_within_radius(begin_y, begin_x, lookup_radius);
            if (from == RoutingKit::invalid_id) {
                continue;
            }
            unsigned to = me.find_nearest_neighbor_within_radius(end_y, end_x, lookup_radius);
            if (to == RoutingKit::invalid_id) {
                continue;
            }
            ch_query.reset().add_source(from).add_target(to).run();
            auto distance = ch_query.get_distance();
            if (distance == RoutingKit::inf_weight)
                continue;
            result.emplace_back(id, tick, begin_x, end_x, begin_y, end_y, from, to, distance);
            break;
        }
    }
    return result;
}
