#ifndef SVGUTILS_UTILS_H
#define SVGUTILS_UTILS_H
#include <cstdlib>
#include <iostream>

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
} // namespace svg

#ifndef NDEBUG
#define unreachable(msg) unreachable_internal(msg, __FILE__, __LINE__)
#else
#define unreachable(msg) unreachable_internal()
#endif // NDEBUG

#endif // SVGUTILS_UTILS_H
