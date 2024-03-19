#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <boost/program_options.hpp>
#include <string>
#include <omp.h>
#include <glaze/glaze.hpp>
#include "ChicagoTaxiTripEngine.h"
#include "MapEngine.h"
#include "MatchingEngine.h"
#include "utils.h"
struct MatchResult {
  uint32_t tick;
  std::vector<std::array<float, 4>> trips;
  std::vector<std::tuple<uint32_t, uint32_t, int64_t, Match>> matches;
};
int main(int argc, char *argv[]) {
    namespace po = boost::program_options;
    namespace fs = std::filesystem;
    fs::path directory;
    uint32_t tries;
    float radius;
    float delta;
    uint32_t max_row_count;
    try {
        po::options_description desc("Options");
        desc.add_options()
            ("help", "Display help message")
            ("data-dir", po::value<fs::path>(&directory)->required(), "Data directory path")
            ("delta", po::value<float>(&delta)->default_value(0.125f), "Percentage of detour tolerance")
            ("max-row", po::value<uint32_t>(&max_row_count)->default_value(std::numeric_limits<uint32_t>::max()), "Maximum row process for each csv file")
            ("max-tries", po::value<uint32_t>(&tries)->default_value(10), "Maximum number of tries when randomize the lat lon coordinates for each trip")
            ("radius", po::value<float>(&radius)->default_value(50.0f), "Radius search for nearest pickup/dropoff point (m)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help") || argc == 1) { // If --help is provided or no arguments are provided
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    } catch (po::error &e) {
        report_and_exit("Error: ", e.what());
    }

    auto ct_directory = directory / "census_tract", csv_directory = directory / "chicago_taxi_csv", ca_directory = directory / "community_area",
        osm_directory = directory / "osm";
    auto directory_check = [](const fs::path &path) {
      return fs::exists(path) && fs::is_directory(path) && !fs::is_empty(path);
    };
    auto file_check = [](const fs::path &path) {
      return fs::exists(path) && fs::is_regular_file(path);
    };
    auto shapefile_check = [&file_check](const fs::path &path) {
      fs::path temp;
      for (const auto &entry : fs::directory_iterator(path)) {
          if (!fs::is_regular_file(entry.path())) continue;
          auto strip_extension = entry.path().stem();
          if (temp.empty()) {
              temp = strip_extension;
          } else if (temp != strip_extension) {
              return false;
          }
      }
      if (!file_check((path / temp).concat(".shp")) || !file_check((path / temp).concat(".dbf"))) return false;
      return true;
    };
    //data layout check
    if (!directory_check(ct_directory) || !shapefile_check(ct_directory) || !directory_check(ca_directory) || !shapefile_check(ca_directory)
        || !directory_check(osm_directory) || !file_check(osm_directory / "Chicago.osm.pbf") || !directory_check(csv_directory)) {
        report_and_exit(
            "Integrity check failed, consider rerun analytic_data_pull.py to fill in the missing file(s):\n",
            !directory_check(ct_directory) ? "missing/empty/denied access " + ct_directory.string() + "\n" :
            !shapefile_check(ct_directory) ? ct_directory.string() + "has multiple file names or missing .shp/.dbf file\n" : "",
            !directory_check(ca_directory) ? "missing/empty/denied access " + ca_directory.string() + "\n" :
            !shapefile_check(ca_directory) ? ca_directory.string() + "has multiple file names or missing .shp/.dbf file\n" : "",
            !directory_check(osm_directory) ? "missing/empty/denied access " + osm_directory.string() + "\n" :
            !file_check(osm_directory / "Chicago.osm.pbf") ? "missing " + (osm_directory / "Chicago.osm.pbf").string() + "\n" : "",
            !directory_check(csv_directory) ? "missing/empty/denied access " + csv_directory.string() + "\n" : ""
        );
    }
    auto shapefile = [](const fs::path &path) {
      for (const auto &entry : fs::directory_iterator(path)) {
          if (!fs::is_regular_file(entry.path())) continue;
          return path / entry.path().filename().replace_extension(".shp");
      }
      report_and_exit("cannot find regular file in " + path.string());
      std::terminate();
    };
    MapEngine map_engine(osm_directory / "Chicago.osm.pbf");
    auto ca = shapefile(ca_directory), ct = shapefile(ct_directory);
    auto result_dir = directory / "result";
    if (!fs::exists(result_dir)) fs::create_directories(result_dir);
    OGRRegisterAll();
#pragma omp parallel shared(map_engine, std::cout) firstprivate(ca, ct, csv_directory, result_dir, delta, max_row_count, tries, radius) default(none)
    {
#pragma omp single
        {
            for (const auto &entry : fs::directory_iterator(csv_directory)) {
                if (!fs::is_regular_file(entry.path()) || entry.path().extension() != ".csv") {
                    continue;
                }
#pragma omp task shared(map_engine, std::cout) firstprivate(entry, ca, ct, result_dir, delta, max_row_count, tries, radius)   default(none)
                {
                    ChicagoTaxiTripEngine trip_engine(entry.path(), ca, ct, max_row_count);
                    auto query_object = map_engine.get_time_query_object();

                    while (true) {
                        auto nxt = trip_engine.next();
                        if (!nxt) break;
                        auto &[day, day_data] = nxt.value();
                        std::stringstream buf;
                        buf << std::put_time(&day, "%Y_%m_%d") << std::fixed << std::setprecision(3) << "_delta=" << delta << ".json";
                        std::vector<MatchResult> day_result;
                        auto generated_data = trip_engine.generate_data(day_data, map_engine, query_object, radius, tries);
                        size_t i = 0, n = generated_data.size();
                        // with every window of the same tick do the oracle matching
                        while (i < n) {
                            size_t j = i + 1;
                            while (j < n && generated_data[j].tick == generated_data[i].tick)
                                j++;
                            std::vector<std::array<float, 4>> trips;
                            trips.reserve(j - i);
                            for (size_t t = i; t < j; t++)
                                trips.push_back({{generated_data[t].begin_x, generated_data[t].begin_y, generated_data[t].end_x, generated_data[t].end_y}});
                            day_result.push_back({generated_data[i].tick, std::move(trips),
                                                  MatchingEngine::offline_optimal_matching(std::span<Trip>(generated_data.begin() + i,
                                                                                                           generated_data.begin() + j),
                                                                                           map_engine,
                                                                                           query_object,
                                                                                           delta
                                                  )});
                            i = j;
                        }
                        std::ofstream day_out(result_dir / buf.str());
                        day_out << glz::write_json(day_result);
                    }
                }
            }
        }
    }
}

