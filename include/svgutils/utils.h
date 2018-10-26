#ifndef SVGUTILS_UTILS_H
#define SVGUTILS_UTILS_H
#include <cctype>
//#include <charconv>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace svg {
[[noreturn]] inline void unreachable_internal(const char *msg = nullptr,
                                              const char *file = nullptr,
                                              unsigned line = 0) {
  if (msg)
    std::cerr << msg << "\n";
  std::cerr << "UNREACHABLE executed";
  if (file)
    std::cerr << " at " << file << ":" << line;
  std::cerr << "!\n";
  std::abort();
}

inline std::string_view strview_trim(std::string_view str) {
  while (std::isspace(str.front()))
    str = str.substr(1);
  while (std::isspace(str.back()))
    str = str.substr(0, str.size() - 1);
  return str;
}

inline std::optional<double> strview_to_double(std::string_view str) {
  // This should work in c++17 but it doesn't on my system (gcc) :(
  // double c;
  // auto fc_res = std::from_chars(val.data(), val.data() + val.size(), c);
  // auto error = fc_res.ec;
  std::string realStr(str.data(), str.size());
  char *parseEnd;
  double d = std::strtod(realStr.c_str(), &parseEnd);
  bool error = parseEnd == realStr.c_str();
  if (error)
    return std::nullopt;
  return d;
}
} // namespace svg

#ifndef NDEBUG
#define unreachable(msg) unreachable_internal(msg, __FILE__, __LINE__)
#else
#define unreachable(msg) unreachable_internal()
#endif // NDEBUG

#endif // SVGUTILS_UTILS_H
