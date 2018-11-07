#ifndef SVGCAIRO_SVG_CAIRO_H
#define SVGCAIRO_SVG_CAIRO_H

#include "svgcairo/freetype.h"
#include "svgutils/css_utils.h"
#include "svgutils/svg_writer.h"

#include <cairo/cairo-ft.h>
#include <cairo/cairo-pdf.h>
#include <filesystem>

namespace svg {

namespace fs = std::filesystem;

class CairoSVGWriter {
public:
  using self_t = CairoSVGWriter;
  using AttrContainer = std::vector<SVGAttribute>;

  enum OutputFormat { PDF, PNG };

  /// Try to extract the document size from the first <svg> tag written later
  /// using the writer. It is possible to set different default output
  /// dimensions using setDefaultWidth and setDefaultHeight.
  CairoSVGWriter(const fs::path &outfile, OutputFormat fmt);
  /// Force the output file to have exactly the dimensioned specified in this
  /// constructor.
  CairoSVGWriter(const fs::path &outfile, OutputFormat fmt, double width,
                 double height);
  CairoSVGWriter(CairoSVGWriter &&) = default;
  CairoSVGWriter &operator=(CairoSVGWriter &&) = default;
  CairoSVGWriter(const CairoSVGWriter &) = delete;
  CairoSVGWriter &operator=(const CairoSVGWriter &) = delete;

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

  void setDefaultWidth(double w) { dfltWidth = w; }
  void setDefaultHeight(double h) { dfltHeight = h; }

private:
  const fs::path outfile;
  const OutputFormat fmt;
  Freetype fonts;
  StyleTracker styles;
  double dfltWidth = 300;
  double dfltHeight = 200;
  double width = 0;
  double height = 0;
  using OwnedSurface =
      std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
  using OwnedCairo = std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;
  OwnedSurface surface = {nullptr, nullptr};
  OwnedCairo cairo = {nullptr, nullptr};

  enum class TagType;
  TagType currentTag;
  std::stack<TagType> parents;

  void openTag(TagType T, const AttrContainer &attrs);
  void closeTag();
  double getWidth() const { return width ? width : dfltWidth; }
  double getHeight() const { return height ? height : dfltHeight; }
  double convertCSSWidth(const CSSUnit &unit) const;
  double convertCSSHeight(const CSSUnit &unit) const;
  void initCairo();
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
