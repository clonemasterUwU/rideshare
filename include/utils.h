#ifndef MODEL_GEN_MK2_INCLUDE_UTILS_H_
#define MODEL_GEN_MK2_INCLUDE_UTILS_H_
#include <charconv>
#include <exception>

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
#endif //MODEL_GEN_MK2_INCLUDE_UTILS_H_
