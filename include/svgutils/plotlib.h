#ifndef SVGUTILS_PLOTLIB_H
#define SVGUTILS_PLOTLIB_H

#include "svg_utils.h"

namespace svg {

template <typename WriterTy>
struct PlotWriter : public ExtendableWriter<PlotWriter<WriterTy>> {
  using base_t = ExtendableWriter<PlotWriter<WriterTy>>;

  PlotWriter(svg::outstream_t &os)
      : base_t(std::make_unique<WriterModel<WriterTy>>(os)) {}

  template <typename... attrs_t>
  PlotWriter &grid(double top, double left, double width, double height,
                   double distx, double disty, attrs_t... attrs) {
    std::vector<SVGAttribute> attrVec({std::forward<attrs_t>(attrs)...});
    return grid(top, left, width, height, distx, disty, attrVec);
  }
  template <typename container_t>
  PlotWriter &grid(double top, double left, double width, double height,
                   double distx, double disty, const container_t &attrs) {
    std::vector<SVGAttribute> attrsExt(attrs.begin(), attrs.end());
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
struct PlotWriterConcept : public WriterConcept {
  virtual PlotWriterConcept &grid(double top, double left, double width,
                                  double height, double distx, double disty,
                                  const std::vector<SVGAttribute> &attrs) = 0;
  template <typename... attrs_t>
  PlotWriterConcept &grid(double top, double left, double width, double height,
                          double distx, double disty, attrs_t... attrs) {
    std::vector<SVGAttribute> attrVec({std::forward<attrs_t>(attrs)...});
    return grid(top, left, width, height, distx, disty, attrVec);
  }
};

template <typename WriterTy>
struct PlotWriterModel : public PlotWriterConcept, WriterModel<WriterTy> {
  template <typename... args_t>
  PlotWriterModel(args_t &&... args)
      : WriterModel<WriterTy>(std::forward<args_t>(args)...) {}
  PlotWriterConcept &grid(double top, double left, double width, double height,
                          double distx, double disty,
                          const std::vector<SVGAttribute> &attrs) override {
    this->WriterModel<WriterTy>::Writer.grid(top, left, width, height, distx,
                                             disty, attrs);
    return *this;
  }
};

struct Plot {
  Plot(std::string name) : name(std::move(name)) {}
  virtual ~Plot() = default;

  virtual double getMinX() const = 0;
  virtual double getMaxX() const = 0;
  virtual double getMinY() const = 0;
  virtual double getMaxY() const = 0;
  virtual void renderPreview(PlotWriterConcept &writer) const = 0;
  virtual void compile(PlotWriterConcept &writer) const = 0;

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

struct Axis {
  virtual ~Axis() = default;
  void addPlot(std::unique_ptr<Plot> plot) {
    plots.emplace_back(std::move(plot));
  }
  double getMinX() const { return minX; }
  double getMaxX() const { return maxX; }
  double getMinY() const { return minY; }
  double getMaxY() const { return maxY; }
  void setLegend(std::unique_ptr<Legend> legend) {
    this->legend = std::move(legend);
  }
  virtual void compile(PlotWriterConcept &writer) const;

private:
  double minX;
  double maxX;
  double minY;
  double maxY;
  std::unique_ptr<Legend> legend;
  std::vector<std::unique_ptr<Plot>> plots;
};

struct CSSRule {
  const char *property;
  std::string value;
};

struct Graph {
  Graph(double width, double height) : width(width), height(height) {}

  void addAxis(std::unique_ptr<Axis> axis) {
    Axes.emplace_back(std::move(axis));
  }
  void addCSSRule(CSSRule rule) { CssRules.emplace_back(std::move(rule)); }

  void compile(PlotWriterConcept &writer) const;

private:
  double width;
  double height;
  std::vector<std::unique_ptr<Axis>> Axes;
  std::vector<CSSRule> CssRules;
};

} // namespace svg
#endif // SVGUTILS_PLOTLIB_H
