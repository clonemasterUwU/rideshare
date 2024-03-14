#ifndef TRIP_H
#define TRIP_H
#include <iomanip>
#include "utils.h"
struct Trip {
    uint32_t id, tick;
    float begin_x, end_x, begin_y, end_y;
    node_t begin_node, end_node;
    travel_time_t time_len;
    //    travel_dist_t dist_len;
    Trip(uint32_t id,
         uint32_t tick,
         float begin_x,
         float end_x,
         float begin_y,
         float end_y,
         node_t begin_node = RoutingKit::invalid_id,
         node_t end_node = RoutingKit::invalid_id,
         travel_time_t time_len = RoutingKit::inf_weight
//         ,travel_dist_t dist_len = RoutingKit::inf_weight
         ) :
        id(id), tick(tick), begin_x(begin_x), end_x(end_x), begin_y(begin_y), end_y(end_y), begin_node(begin_node), end_node(end_node), time_len(time_len)
//        ,dist_len(dist_len)
    {}
};
#endif // TRIP_H
