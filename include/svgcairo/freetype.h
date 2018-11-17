#ifndef SVGCAIRO_FREETYPE_H
#define SVGCAIRO_FREETYPE_H

#include <memory>
#include <optional>

struct FT_FaceRec_;

namespace svg {
struct Freetype {
  Freetype(Freetype &&);
  Freetype &operator=(Freetype &&);
  ~Freetype();

  using FT_Face = FT_FaceRec_ *;
  FT_Face getFace(const char *desc);

  static std::optional<Freetype> Create();
  struct Context;
private:
  std::unique_ptr<Context> ctx;
  Freetype(std::unique_ptr<Context> ctx);
};

} // namespace svg
#endif // SVGCAIRO_FREETYPE_H
