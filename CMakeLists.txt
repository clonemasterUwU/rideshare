cmake_minimum_required(VERSION 3.25)
project(rideshare)
set(CMAKE_CXX_STANDARD 20)


include_directories(include)

find_package(GDAL CONFIG)

add_executable(analytic
        src/routingkit/bit_select.cpp
        src/routingkit/bit_vector.cpp
        src/routingkit/buffered_asynchronous_reader.cpp
        src/routingkit/contraction_hierarchy.cpp
        src/routingkit/customizable_contraction_hierarchy.cpp
        src/routingkit/file_data_source.cpp
        src/routingkit/geo_position_to_node.cpp
        src/routingkit/graph_util.cpp
        src/routingkit/id_mapper.cpp
        src/routingkit/nested_dissection.cpp
        src/routingkit/osm_decoder.cpp
        src/routingkit/osm_graph_builder.cpp
        src/routingkit/osm_profile.cpp
        src/routingkit/osm_simple.cpp
        src/routingkit/protobuf.cpp
        src/routingkit/strongly_connected_component.cpp
        src/routingkit/timer.cpp
        src/routingkit/vector_io.cpp
        src/MapEngine.cpp
        src/MatchingEngine.cpp
        src/ChicagoTaxiTripEngine.cpp
        src/ChicagoAnalytic.cpp
)

add_executable(server
        src/server.cpp
        src/MapEngine.cpp
        src/MatchingEngine.cpp
        src/routingkit/bit_select.cpp
        src/routingkit/bit_vector.cpp
        src/routingkit/buffered_asynchronous_reader.cpp
        src/routingkit/contraction_hierarchy.cpp
        src/routingkit/customizable_contraction_hierarchy.cpp
        src/routingkit/file_data_source.cpp
        src/routingkit/geo_position_to_node.cpp
        src/routingkit/graph_util.cpp
        src/routingkit/id_mapper.cpp
        src/routingkit/nested_dissection.cpp
        src/routingkit/osm_decoder.cpp
        src/routingkit/osm_graph_builder.cpp
        src/routingkit/osm_profile.cpp
        src/routingkit/osm_simple.cpp
        src/routingkit/protobuf.cpp
        src/routingkit/strongly_connected_component.cpp
        src/routingkit/timer.cpp
        src/routingkit/vector_io.cpp
)


target_compile_options(analytic PRIVATE -g -fsanitize=address -fsanitize=undefined -O2)
target_link_options(analytic PRIVATE -g -fsanitize=address -fsanitize=undefined)

target_link_options(server PRIVATE -g -fsanitize=address -fsanitize=undefined -O2)
target_link_options(server PRIVATE -g -fsanitize=address -fsanitize=undefined)

#fight with apple linker to have openmp
if (APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include -lomp")
else ()
    find_package(OpenMP REQUIRED)
    target_link_libraries(analytic PRIVATE OpenMP::OpenMP_CXX)
    target_link_libraries(server PRIVATE OpenMP::OpenMP_CXX)
endif ()

if (GDAL_FOUND)
    target_link_libraries(analytic PRIVATE GDAL::GDAL)
endif ()

find_package(ZLIB REQUIRED)
target_link_libraries(analytic PRIVATE ZLIB::ZLIB)
target_link_libraries(server PRIVATE ZLIB::ZLIB)

find_package(Threads)
if (Threads_FOUND)
    target_link_libraries(server PRIVATE Threads::Threads)
endif ()

find_package(OpenSSL)
if (OpenSSL_FOUND)
    target_link_libraries(server PRIVATE OpenSSL::SSL)
endif ()

find_package(Boost COMPONENTS program_options)
if (Boost_FOUND)
    target_link_libraries(analytic PRIVATE Boost::program_options)
    target_link_libraries(server PRIVATE Boost::program_options)
endif ()



find_package(spdlog)
if (spdlog_FOUND)
    target_link_libraries(analytic PRIVATE spdlog::spdlog)
    target_link_libraries(server PRIVATE spdlog::spdlog)
endif ()

include(FetchContent)

FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG main
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glaze)

target_link_libraries(server PRIVATE glaze::glaze)
target_link_libraries(analytic PRIVATE glaze::glaze)

