#ifndef SVGCAIRO_SVG_CAIRO_H
#define SVGCAIRO_SVG_CAIRO_H

#include "svgutils/svg_utils.h"

namespace svg {

/// Base implementation of a writer for svg documents.
/// Allows overriding most member functions using CRTP.
class CairoSVGWriter {
public:
  using self_t = CairoSVGWriter;
  using AttrContainer = std::vector<SVGAttribute>;
  CairoSVGWriter(outstream_t &output) : outstream(&output) {}

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> self_t &NAME(attrs_t... attrs) {              \
    AttrContainer attrsVec({std::forward<attrs_t>(attrs)...});                    \
    NAME(attrsVec);                                                               \
    return *this;                                                              \
  }                                                                            \
  self_t &NAME(const AttrContainer &attrs);
#include "svgutils/svg_entities.def"

  self_t &content(const char *text);

  self_t &enter();
  self_t &leave();
  self_t &finish();

private:
  outstream_t *outstream;
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
