#ifndef SVGCAIRO_FREETYPE_H
#define SVGCAIRO_FREETYPE_H

#include <memory>
#include <optional>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace svg {
struct Freetype {
  Freetype(Freetype &&);
  Freetype &operator=(Freetype &&);
  ~Freetype();

  FT_Face getFace(const char *desc);

  static std::optional<Freetype> Create();
  struct Context;
private:
  std::unique_ptr<Context> ctx;
  Freetype(std::unique_ptr<Context> ctx);
};

} // namespace svg
#endif // SVGCAIRO_FREETYPE_H
