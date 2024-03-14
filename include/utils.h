#ifndef UTILS_H_
#define UTILS_H_
#include <charconv>
#include <stdexcept>
#include <iostream>
#include "routingkit/constants.h"
#include "spdlog/spdlog.h"

#ifdef NDEBUG
#define ASSERT(condition) ((void)0)
#else
#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error("Assertion failed: " + std::string(#condition) + " in " + __FILE__ + " at line " + std::to_string(__LINE__)); \
        } \
    } while (false)
#endif

template<class T, class... Args>
void from_chars_throws(const char *first,
                       const char *last,
                       T &t,
                       Args... args) {
  std::from_chars_result res = std::from_chars(first, last, t, args...);

  // These two exceptions reflect the behavior of std::stoi.
  if (res.ec == std::errc::invalid_argument) {
    throw std::invalid_argument{"invalid_argument"};
  } else if (res.ec == std::errc::result_out_of_range) {
    throw std::out_of_range{"out_of_range"};
  }
}

template <typename T>
auto report_and_exit(const T &message) {
    std::cerr << message << std::endl;
    std::terminate();
}

template <typename T, typename... Args>
auto report_and_exit(const T &message, Args&&... args) {
    std::cerr << message;
    report_and_exit(std::forward<Args>(args)...);
}

using node_t = decltype(RoutingKit::invalid_id);
using travel_time_t = decltype(RoutingKit::inf_weight);
using travel_dist_t = decltype(RoutingKit::inf_weight);
#endif //UTILS_H_
