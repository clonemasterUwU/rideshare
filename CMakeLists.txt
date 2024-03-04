cmake_minimum_required(VERSION 3.26)
project(model_gen_mk2)
set(CMAKE_CXX_STANDARD 20)


include_directories(include RoutingKit/include)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GDAL CONFIG REQUIRED)

add_executable(model_gen_mk2 main.cpp
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
        src/TripEngine.cpp
)
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp -L/opt/homebrew/opt/libomp/lib -I/opt/homebrew/opt/libomp/include -lomp")
else ()
    find_package(OpenMP REQUIRED)
    target_link_libraries(model_gen_mk2 PRIVATE OpenMP::OpenMP_CXX)
endif ()

target_compile_options(model_gen_mk2 PRIVATE -g -O2)
target_compile_options(model_gen_mk2 PRIVATE -g -fsanitize=address -fsanitize=undefined -O2)
target_link_options(model_gen_mk2 PRIVATE -g -fsanitize=address -fsanitize=undefined)
target_link_libraries(model_gen_mk2 PRIVATE GDAL::GDAL
        ZLIB::ZLIB
        Threads::Threads
)

