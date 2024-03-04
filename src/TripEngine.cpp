//
// Created by Hiep Trinh on 3/1/24.
//
#include "TripEngine.h"
Trip::Trip(uint32_t tick, int64_t begin_census_or_ca, int64_t end_census_or_ca)
    :
    tick(tick),
    begin_census_or_ca(begin_census_or_ca),
    end_census_or_ca(end_census_or_ca) {}
