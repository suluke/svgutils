#ifndef SVGUTILS_PLOTLIB_CORE_H
#define SVGUTILS_PLOTLIB_CORE_H

#include "css_utils.h"
#include "svg_utils.h"
#include <functional>
#include <optional>
#include <string_view>

namespace plots {
template <typename WriterTy>
struct PlotWriter : public svg::ExtendableWriter<PlotWriter<WriterTy>> {
  using base_t = svg::ExtendableWriter<PlotWriter<WriterTy>>;

  template <typename... args_t>
  PlotWriter(args_t &&... args)
      : base_t(std::make_unique<svg::WriterModel<WriterTy>>(
            std::forward<args_t>(args)...)) {}

  template <typename... attrs_t>
  PlotWriter &grid(double top, double left, double width, double height,
                   double distx, double disty, attrs_t... attrs) {
    std::vector<svg::SVGAttribute> attrVec({std::forward<attrs_t>(attrs)...});
    return grid(top, left, width, height, distx, disty, attrVec);
  }
  template <typename container_t>
  PlotWriter &grid(double top, double left, double width, double height,
                   double distx, double disty, const container_t &attrs) {
    using namespace svg;
    std::vector<svg::SVGAttribute> attrsExt(attrs.begin(), attrs.end());
    auto &self = static_cast<base_t &>(*this);
    for (double i = top; i <= top + height; i += disty) {
      attrsExt.erase(attrsExt.begin() + attrs.size(), attrsExt.end());
      attrsExt.insert(attrsExt.end(),
                      {x1(left), y1(i), x2(left + width), y2(i)});
      self.line(attrsExt);
    }
    for (double i = left; i < left + width; i += distx) {
      attrsExt.erase(attrsExt.begin() + attrs.size(), attrsExt.end());
      attrsExt.insert(attrsExt.end(),
                      {x1(i), y1(top), x2(i), y2(top + height)});
      self.line(attrsExt);
    }
    return *this;
  }
};

struct PlotWriterConcept : public virtual svg::WriterConcept {
  virtual PlotWriterConcept &
  grid(double top, double left, double width, double height, double distx,
       double disty, const std::vector<svg::SVGAttribute> &attrs) = 0;
  template <typename... attrs_t>
  PlotWriterConcept &grid(double top, double left, double width, double height,
                          double distx, double disty, attrs_t... attrs) {
    std::vector<svg::SVGAttribute> attrVec({std::forward<attrs_t>(attrs)...});
    return grid(top, left, width, height, distx, disty, attrVec);
  }
};

template <typename WriterTy>
struct PlotWriterModel : public PlotWriterConcept,
                         public svg::WriterModel<WriterTy> {
  using ModelBase_t = svg::WriterModel<WriterTy>;
  template <typename... args_t>
  PlotWriterModel(args_t &&... args)
      : ModelBase_t(std::forward<args_t>(args)...) {}
  PlotWriterConcept &
  grid(double top, double left, double width, double height, double distx,
       double disty, const std::vector<svg::SVGAttribute> &attrs) override {
    this->ModelBase_t::Writer.grid(top, left, width, height, distx, disty,
                                   attrs);
    return *this;
  }
};

struct Axis;

struct Plot {
  Plot(std::string name) : name(std::move(name)) {}
  virtual ~Plot() = default;

  virtual double getMinX() const = 0;
  virtual double getMaxX() const = 0;
  virtual double getMinY() const = 0;
  virtual double getMaxY() const = 0;
  virtual void renderPreview(PlotWriterConcept &writer) const = 0;
  virtual void compile(PlotWriterConcept &writer, const Axis &axis) const = 0;

  const std::string &getName() const { return name; }

private:
  std::string name;
};

struct Legend {
  virtual ~Legend() = default;
  virtual double getWidth() const = 0;
  virtual double getHeight() const = 0;
  virtual void addPlot(Plot *plot) = 0;
  virtual void compile(PlotWriterConcept &writer) const = 0;
};

template <unsigned dim, typename data_t = double> struct Point {
  std::array<data_t, dim> dimensions;

  constexpr Point() = default;
  template <typename... args_t>
  constexpr Point(args_t &&... args)
      : dimensions({{std::forward<args_t>(args)...}}){};

  constexpr data_t x() const {
    static_assert(dim > 0,
                  "Cannot get first dimension of vector with zero dimensions");
    return dimensions[0];
  }
  constexpr data_t y() const {
    static_assert(
        dim > 1,
        "Cannot get second dimension of vector with less than two dimensions");
    return dimensions[1];
  }
  constexpr void x(data_t val) {
    static_assert(dim > 0,
                  "Cannot set first dimension of vector with zero dimensions");
    dimensions[0] = val;
  }
  constexpr void y(data_t val) {
    static_assert(
        dim > 1,
        "Cannot set second dimension of vector with less than two dimensions");
    dimensions[1] = val;
  }

  constexpr auto begin() { return dimensions.begin(); }
  constexpr auto end() { return dimensions.end(); }
  constexpr auto begin() const { return dimensions.begin(); }
  constexpr auto end() const { return dimensions.end(); }

  constexpr unsigned size() const { return dim; }

  template <unsigned odim>
  constexpr auto operator+(const Point<odim, data_t> &o) const {
    constexpr unsigned maxDim = dim > odim ? dim : odim;
    Point<maxDim, data_t> p;
    auto outIt = p.begin();
    for (auto D : dimensions)
      *(outIt++) = D;
    outIt = p.begin();
    for (auto D : o.dimensions)
      *(outIt++) = D;
    return p;
  }
  template <unsigned odim>
  constexpr auto operator-(const Point<odim, data_t> &o) const {
    return *this + (o * -1.);
  }
  constexpr Point<dim, data_t> operator+(data_t o) const {
    auto p = *this;
    for (data_t &d : p)
      d += o;
    return p;
  }
  constexpr Point<dim, data_t> operator-(data_t o) const {
    return *this + (-o);
  }
  constexpr Point<dim, data_t> operator*(data_t o) const {
    auto p = *this;
    for (data_t &d : p)
      d *= o;
    return p;
  }
};

/// Simple base implementation of an interface to query font styles and
/// text dimensions
struct FontInfo {
  FontInfo() = default;
  FontInfo(std::string font, double fontSize)
      : font(std::move(font)), fontSize(fontSize) {}
  FontInfo(const FontInfo &) = default;
  FontInfo(FontInfo &&) = default;
  FontInfo &operator=(const FontInfo &) = default;
  FontInfo &operator=(FontInfo &&) = default;

  void setFont(std::string font) { font = std::move(font); }
  void setSize(double s) { fontSize = s; }
  double getSize() const { return fontSize; }

  virtual std::string getFontStyle() const;
  virtual double getWidth(std::string_view text, bool multiLine = false) const;
  virtual double getHeight(std::string_view text, bool multiLine = false) const;
  virtual void placeText(const char *text, PlotWriterConcept &writer,
                         Point<2> position, Point<2> anchor = {0., 0.},
                         bool multiLine = false) const;

private:
  std::string font = "Times, serif";
  double fontSize = 12;
};

struct AxisStyle {
  using TickGen = std::function<std::string(double coord)>;
  enum Type { OUTER, INNER } type = OUTER;
  bool grid = true;
  double minX = 0;
  double maxX;
  double minY = 0;
  double maxY;
  double xStep = 1.;
  double yStep = 1.;
  std::string xLabel = "x";
  std::string yLabel = "y";
  TickGen xTickGen = [](double x) { return std::to_string(x); };
  TickGen yTickGen = [](double y) { return std::to_string(y); };
};

struct Graph;

struct Axis {
  virtual ~Axis() = default;
  template <typename PlotTy> PlotTy *addPlot(std::unique_ptr<PlotTy> plot) {
    plots.emplace_back(std::move(plot));
    return static_cast<PlotTy *>(plots.back().get());
  }
  void setLegend(std::unique_ptr<Legend> legend) {
    this->legend = std::move(legend);
  }
  virtual void compile(PlotWriterConcept &writer, const Graph &graph,
                       double width, double height);
  virtual Point<2> project(Point<2> p) const;
  AxisStyle &prepareStyle();
  AxisStyle &getStyle();
  const AxisStyle &getStyle() const {
    assert(style);
    return *style;
  }

private:
  double width;
  double height;
  Point<2> translation;
  std::unique_ptr<Legend> legend;
  std::vector<std::unique_ptr<Plot>> plots;
  std::optional<AxisStyle> style;
};

struct Graph {
  Graph(double width, double height, std::unique_ptr<FontInfo> font = nullptr)
      : width(width), height(height),
        font(font ? std::move(font) : std::make_unique<FontInfo>()) {}

  template <typename AxisTy> AxisTy *addAxis(std::unique_ptr<AxisTy> axis) {
    Axes.emplace_back(std::move(axis));
    return static_cast<AxisTy *>(Axes.back().get());
  }
  void addCSSRule(svg::CSSRule rule) { CssRules.emplace_back(std::move(rule)); }
  FontInfo &getFontInfo() const { return *font; }

  void compile(PlotWriterConcept &writer) const;

private:
  double width;
  double height;
  std::vector<std::unique_ptr<Axis>> Axes;
  std::vector<svg::CSSRule> CssRules;
  std::unique_ptr<FontInfo> font;
};

} // namespace plots
#endif // SVGUTILS_PLOTLIB_CORE_H
