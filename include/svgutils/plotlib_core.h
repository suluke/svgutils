#ifndef SVGUTILS_PLOTLIB_CORE_H
#define SVGUTILS_PLOTLIB_CORE_H

#include "svg_utils.h"

namespace plots {
template <typename WriterTy>
struct PlotWriter : public svg::ExtendableWriter<PlotWriter<WriterTy>> {
  using base_t = svg::ExtendableWriter<PlotWriter<WriterTy>>;

  PlotWriter(svg::outstream_t &os)
      : base_t(std::make_unique<svg::WriterModel<WriterTy>>(os)) {}

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
};

struct Axis {
  virtual ~Axis() = default;
  template <typename PlotTy> PlotTy *addPlot(std::unique_ptr<PlotTy> plot) {
    plots.emplace_back(std::move(plot));
    return static_cast<PlotTy *>(plots.back().get());
  }
  double getMinX() const { return minX; }
  double getMaxX() const { return maxX; }
  double getMinY() const { return minY; }
  double getMaxY() const { return maxY; }
  void setLegend(std::unique_ptr<Legend> legend) {
    this->legend = std::move(legend);
  }
  virtual void compile(PlotWriterConcept &writer, double width, double height);
  virtual Point<2> project(Point<2> p) const;

private:
  void updateBounds(double width, double height);

  static inline constexpr double infty =
      std::numeric_limits<double>::infinity();
  double minX = infty;
  double maxX = -infty;
  double minY = infty;
  double maxY = -infty;
  double width;
  double height;
  std::unique_ptr<Legend> legend;
  std::vector<std::unique_ptr<Plot>> plots;
};

struct CSSRule {
  const char *property;
  std::string value;
};

struct Graph {
  Graph(double width, double height) : width(width), height(height) {}

  template <typename AxisTy> AxisTy *addAxis(std::unique_ptr<AxisTy> axis) {
    Axes.emplace_back(std::move(axis));
    return static_cast<AxisTy *>(Axes.back().get());
  }
  void addCSSRule(CSSRule rule) { CssRules.emplace_back(std::move(rule)); }

  void compile(PlotWriterConcept &writer) const;

private:
  double width;
  double height;
  std::vector<std::unique_ptr<Axis>> Axes;
  std::vector<CSSRule> CssRules;
};

} // namespace plots
#endif // SVGUTILS_PLOTLIB_CORE_H
