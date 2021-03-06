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

inline std::string_view strview_trim(const std::string_view &str) {
  std::string_view res = str;
  while (res.size() && std::isspace(res.front()))
    res.remove_prefix(1);
  while (res.size() && std::isspace(res.back()))
    res.remove_suffix(1);
  return res;
}

inline std::optional<double> strview_to_double(std::string_view str) {
  // This should work in c++17 but it doesn't on my system (gcc) :(
  // double d;
  // auto fc_res = std::from_chars(str.data(), str.data() + str.size(), d);
  // auto error = fc_res.ec;
  std::string realStr(str.data(), str.size());
  char *parseEnd;
  double d = std::strtod(realStr.c_str(), &parseEnd);
  bool error = parseEnd == realStr.c_str();
  if (error)
    return std::nullopt;
  return d;
}

template <typename container_t>
inline void strview_split(std::string_view str, std::string_view splitchars,
                          /* out */ container_t &splits) {
  do {
    size_t splitpos = str.find_first_of(splitchars);
    if (splitpos == std::string_view::npos) {
      splits.emplace_back(str.substr(0, str.size()));
      str = str.substr(str.size(), 0);
    } else {
      splits.emplace_back(str.substr(0, splitpos));
      str = str.substr(splitpos + 1, str.size() - splitpos);
    }
  } while (str.size());
}
} // namespace svg

#ifndef NDEBUG
#define svg_unreachable(msg) unreachable_internal(msg, __FILE__, __LINE__)
#else
#define svg_unreachable(msg) unreachable_internal()
#endif // NDEBUG

#endif // SVGUTILS_UTILS_H
