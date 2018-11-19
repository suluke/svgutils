#ifndef SVGCAIRO_SVG_CAIRO_H
#define SVGCAIRO_SVG_CAIRO_H

#include "svgcairo/freetype.h"
#include "svgutils/css_utils.h"
#include "svgutils/svg_writer.h"

#include <filesystem>

struct _cairo;
struct _cairo_surface;

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
    AttrContainer attrsVec({std::forward<attrs_t>(attrs)...});                 \
    NAME(attrsVec);                                                            \
    return *this;                                                              \
  }                                                                            \
  template <typename container_t> self_t &NAME(const container_t &attrs) {     \
    AttrContainer attrsVec(attrs.begin(), attrs.end());                        \
    NAME(attrsVec);                                                            \
    return *this;                                                              \
  }                                                                            \
  self_t &NAME(const AttrContainer &attrs);
#include "svgutils/svg_entities.def"
  template <typename... attrs_t>
  self_t &custom_tag(const char *name, attrs_t...) {
    return custom_tag(name, std::vector<SVGAttribute>{});
  }
  template <typename container_t>
  self_t &custom_tag(const char *name, const container_t &) {
    return custom_tag(name, std::vector<SVGAttribute>{});
  }
  self_t &custom_tag(const char *name, const std::vector<SVGAttribute> &);

  self_t &content(const char *text);
  self_t &comment(const char *comment);

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
  using SurfaceDestroyTy = void(*)(_cairo_surface *);
  using OwnedSurface =
      std::unique_ptr<_cairo_surface, SurfaceDestroyTy>;
  using CairoDestroyTy = void(*)(_cairo *);
  using OwnedCairo = std::unique_ptr<_cairo, CairoDestroyTy>;
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
  void applyCSSFill(bool preserve);
  void applyCSSStroke(bool preserve);
  void applyCSSFillAndStroke(bool preserve);
  void initCairo();

  bool CairoExecuteHLine(std::string_view length, bool rel);
  bool CairoExecuteVLine(std::string_view length, bool rel);
  bool CairoExecuteLineTo(std::string_view points, bool rel);
  bool CairoExecuteMoveTo(std::string_view points, bool rel);
  bool CairoExecutePath(const char *pathRaw);
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
