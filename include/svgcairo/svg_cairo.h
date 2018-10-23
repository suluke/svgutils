#ifndef SVGCAIRO_SVG_CAIRO_H
#define SVGCAIRO_SVG_CAIRO_H

#include "svgutils/svg_utils.h"

#include <cairo/cairo-pdf.h>
#include <filesystem>

namespace svg {

namespace fs = std::filesystem;

/// Base implementation of a writer for svg documents.
/// Allows overriding most member functions using CRTP.
class CairoSVGWriter {
public:
  using self_t = CairoSVGWriter;
  using AttrContainer = std::vector<SVGAttribute>;
  CairoSVGWriter(const fs::path &outfile, double width, double height);

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> self_t &NAME(attrs_t... attrs) {              \
    AttrContainer attrsVec = {std::forward<attrs_t>(attrs)...};                \
    NAME(attrsVec);                                                            \
    return *this;                                                              \
  }                                                                            \
  self_t &NAME(const AttrContainer &attrs);
#include "svgutils/svg_entities.def"

  self_t &content(const char *text);

  self_t &enter();
  self_t &leave();
  self_t &finish();

private:
  fs::path outfile;
  std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface;
  std::unique_ptr<cairo_t, decltype(&cairo_destroy)> cairo;
  double width;
  double height;
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
