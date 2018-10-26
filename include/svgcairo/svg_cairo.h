#ifndef SVGCAIRO_SVG_CAIRO_H
#define SVGCAIRO_SVG_CAIRO_H

#include "svgutils/svg_utils.h"
#include "svgutils/css_utils.h"

#include <cairo/cairo-pdf.h>
#include <filesystem>

namespace svg {

namespace fs = std::filesystem;

class CairoSVGWriter {
public:
  using self_t = CairoSVGWriter;
  using AttrContainer = std::vector<SVGAttribute>;
  CairoSVGWriter(const fs::path &outfile, double width, double height);
  CairoSVGWriter(CairoSVGWriter &&) = default;
  CairoSVGWriter &operator=(CairoSVGWriter &&) = default;

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
  enum class TagType;
  TagType currentTag;
  std::stack<TagType> parents;
  fs::path outfile;
  std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface;
  std::unique_ptr<cairo_t, decltype(&cairo_destroy)> cairo;
  StyleTracker styles;
  double width;
  double height;

  void openTag(TagType T, const AttrContainer &attrs);
  void closeTag();
  double convertCSSWidth(const CSSUnit &unit) const;
  double convertCSSHeight(const CSSUnit &unit) const;
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
