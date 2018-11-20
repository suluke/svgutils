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
  self_t &NAME(const AttrContainer &attrs);                                    \
  void NAME##_impl(const AttrContainer &attrs);
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

  struct PathError {
    explicit PathError(const std::string &what) : msg(what) {}
    PathError(const PathError &) = default;
    PathError &operator=(const PathError &) = default;
    PathError(PathError &&) = default;
    PathError &operator=(PathError &&) = default;
    const std::string &what() const { return msg; }
  private:
    std::string msg;
  };
  template<typename T>
  struct PathErrorOr {
    // Initialize-from-value ctors
    PathErrorOr(const T &val) : content(val) {}
    PathErrorOr(T &&val) : content(std::move(val)) {}
    PathErrorOr(const PathError &err) : content(err) {}
    PathErrorOr(PathError &&err) : content(std::move(err)) {}
    // Default/Copy/Assignment ctors
    PathErrorOr() = delete;
    PathErrorOr(const PathErrorOr &) = default;
    PathErrorOr &operator=(const PathErrorOr &) = default;
    PathErrorOr(PathErrorOr &&) = default;
    PathErrorOr &operator=(PathErrorOr &&) = default;
    // Inspection
    operator bool() const { return std::holds_alternative<PathError>(content); }
    const T& operator*() const { return std::get<T>(content); }
    operator const T&() const { return std::get<T>(content); }
    const PathError &to_error() const { return std::get<PathError>(content); }
  private:
    std::variant<PathError, T> content;
  };
  struct PathErrorOrVoid {
    PathErrorOrVoid() = default;
    PathErrorOrVoid(const PathError &err) : err(err) {}
    PathErrorOrVoid(PathError &&err) : err(std::move(err)) {}
    // Default/Copy/Assignment ctors
    PathErrorOrVoid(const PathErrorOrVoid &) = default;
    PathErrorOrVoid &operator=(const PathErrorOrVoid &) = default;
    PathErrorOrVoid(PathErrorOrVoid &&) = default;
    PathErrorOrVoid &operator=(PathErrorOrVoid &&) = default;
    // Inspection
    operator bool() const { return !!err; }
    const PathError &to_error() const { return *err; }
  private:
    std::optional<PathError> err;
  };

  using ControlPoint = std::pair<double, double>;

  PathErrorOr<ControlPoint> CairoExecuteCubicBezier(std::string_view args, bool rel);
  PathErrorOr<ControlPoint> CairoExecuteSmoothCubicBezier(std::string_view args, bool rel, const std::optional<ControlPoint> &PrevCP);
  PathErrorOr<ControlPoint> CairoExecuteQuadraticBezier(std::string_view args, bool rel);
  PathErrorOr<ControlPoint> CairoExecuteSmoothQuadraticBezier(std::string_view args, bool rel, const std::optional<ControlPoint> &PrevCP);
  PathErrorOrVoid CairoExecuteArc(std::string_view args, bool rel);
  PathErrorOrVoid CairoExecuteHLine(std::string_view length, bool rel);
  PathErrorOrVoid CairoExecuteVLine(std::string_view length, bool rel);
  PathErrorOrVoid CairoExecuteLineTo(std::string_view points, bool rel);
  PathErrorOrVoid CairoExecuteMoveTo(std::string_view points, bool rel);
  PathErrorOrVoid CairoExecutePath(const char *pathRaw);
};
} // namespace svg
#endif // SVGCAIRO_SVG_CAIRO_H
