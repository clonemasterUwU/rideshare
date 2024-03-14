#ifndef INCLUDE_JSONIFY_H_
#define INCLUDE_JSONIFY_H_
#include <vector>
#include <iostream>
#include <tuple>
#include "Trip.h"
#include "MatchingEngine.h"
template<class Tuple, std::size_t N>
struct TuplePrinter {
  static void print(std::ostream &os, const Tuple &t) {
      TuplePrinter<Tuple, N - 1>::print(os, t);
      os << "," << std::get<N - 1>(t);
  }
};

template<class Tuple>
struct TuplePrinter<Tuple, 1> {
  static void print(std::ostream &os, const Tuple &t) {
      os << std::get<0>(t);
  }
};

template<typename... Args, std::enable_if_t<sizeof...(Args) == 0, int> = 0>
std::ostream &operator<<(std::ostream &os, const std::tuple<Args...> &t) {
    os << "[]";
    return os;
}

template<typename... Args, std::enable_if_t<sizeof...(Args) != 0, int> = 0>
std::ostream &operator<<(std::ostream &os, const std::tuple<Args...> &t) {
    os << "[";
    TuplePrinter<decltype(t), sizeof...(Args)>::print(os, t);
    os << "]";
    return os;
}

template<typename... Args, std::enable_if_t<sizeof...(Args) != 0, int> = 0>
void print(const std::tuple<Args...> &t) {
    std::cout << "[";
    TuplePrinter<decltype(t), sizeof...(Args)>::print(t);
    std::cout << "]";
}
std::ostream &operator<<(std::ostream &os, Match m) {
    os << (m == ABAB ? "\"ABAB\"" : m == ABBA ? "\"ABBA\"" : m == BAAB ? "\"BAAB\"" : m == BABA ? "\"BABA\"" : "\"\"");
    return os;
}
template<typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
    os << "[";
    if (!vec.empty()) {
        auto iter = vec.begin();
        os << *iter;
        ++iter;
        for (; iter != vec.end(); ++iter) {
            os << "," << *iter;
        }
    }
    os << "]";
    return os;
}

std::ostream &operator<<(std::ostream &os, const MatchResult &match_result) {
    os << "{\"graph edges\":" << match_result.graph_edges << ","
       << "\"matches\":" << match_result.matches << ","
       << "\"original routes\":" << match_result.original_routes << ","
       << "\"new routes\":" << match_result.new_routes << "}";
    return os;
}
#endif //INCLUDE_JSONIFY_H_
