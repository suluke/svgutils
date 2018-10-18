#include "svgutils/plotlib.h"
#include <algorithm>
#include <limits>

using namespace svg;
using namespace plots;

template <typename container_t>
static std::string ConcatStyles(const container_t &styles) {
  std::stringstream ss;
  for (const CSSRule &Rule : styles)
    ss << Rule.property << ": " << Rule.value << ";";
  return ss.str();
}

void Axis::compile(PlotWriterConcept &writer, double width,
                   double height) const {
  double infty = std::numeric_limits<double>::infinity();
  double minX = infty, maxX = -infty, minY = infty, maxY = -infty;
  for (const auto &Plot : plots) {
    minX = std::min(Plot->getMinX(), minX);
    maxX = std::max(Plot->getMaxX(), maxX);
    minY = std::min(Plot->getMinY(), minY);
    maxY = std::max(Plot->getMaxY(), maxY);
  }
  using namespace svg;
  std::string trans;
  {
    std::stringstream ss;
    ss << "translate(0, " << height << ") scale(" << width / (maxX - minX)
       << ", -" << height / (maxY - minY) << ")";
    trans = ss.str();
  }
  writer.g(x(0), y(0), transform(trans.c_str()));
  writer.enter();
  for (const auto &Plot : plots)
    Plot->compile(writer, *this);
  writer.leave();
}
Point<2> Axis::project(Point<2> p) const {
  p.x(p.x());
  p.y(p.y());
  return p;
}

void Graph::compile(PlotWriterConcept &writer) const {
  using namespace svg;
  std::string vBox;
  {
    std::stringstream ss;
    ss << 0 << " " << 0 << " " << width << " " << height;
    vBox = ss.str();
  }
  writer.svg(viewBox(vBox.c_str()), preserveAspectRatio("none"),
             style(ConcatStyles(CssRules).c_str()));
  writer.enter();
  for (const auto &Axis : Axes)
    Axis->compile(writer, width, height);
  writer.leave();
}

static void writeHLine(PlotWriterConcept &writer, const Axis &axis, double x,
                       double y, double width) {
  using namespace svg;
  Point<2> Left = axis.project(Point<2>(x - width / 2, y));
  Point<2> Right = axis.project(Point<2>(x + width / 2, y));
  writer.line(x1(Left.x()), y1(Left.y()), x2(Right.x()), y2(Right.y()),
              svg::style("stroke: black;stroke-width: 0.1;"));
}

static void writeVLine(PlotWriterConcept &writer, const Axis &axis, double x,
                       double y1, double y2) {
  Point<2> Top = axis.project(Point<2>(x, y1));
  Point<2> Bottom = axis.project(Point<2>(x, y2));
  writer.line(x1(Top.x()), svg::y1(Top.y()), x2(Bottom.x()),
              svg::y2(Bottom.y()),
              svg::style("stroke: black;stroke-width: 0.05;"));
}

void BoxPlotData::compile(PlotWriterConcept &writer, const Axis &axis,
                          const BoxStyle &style, size_t x) const {
  writeHLine(writer, axis, x, topWhisker, style.topWhiskerWidth);
  writeHLine(writer, axis, x, upperQuartile, style.boxWidth);
  writeHLine(writer, axis, x, median, style.boxWidth);
  writeHLine(writer, axis, x, lowerQuartile, style.boxWidth);
  writeHLine(writer, axis, x, bottomWhisker, style.bottomWhiskerWidth);
  writeVLine(writer, axis, x, bottomWhisker, lowerQuartile);
  writeVLine(writer, axis, x - style.boxWidth / 2 + 0.025, lowerQuartile,
             upperQuartile);
  writeVLine(writer, axis, x + style.boxWidth / 2 - 0.025, lowerQuartile,
             upperQuartile);
  writeVLine(writer, axis, x, upperQuartile, topWhisker);
}

double BoxPlot::getMinX() const { return 0; }
double BoxPlot::getMaxX() const { return data.size() + 1; }
double BoxPlot::getMinY() const {
  double min = std::numeric_limits<double>::infinity();
  for (const BoxPlotData &Box : data)
    min = std::min(Box.bottomWhisker, min);
  return min;
}
double BoxPlot::getMaxY() const {
  double max = -std::numeric_limits<double>::infinity();
  for (const BoxPlotData &Box : data)
    max = std::max(Box.topWhisker, max);
  return max;
}
void BoxPlot::renderPreview(PlotWriterConcept &writer) const {
  writer.rect(x(0), y(0), width("100%"), height("100%"));
}
void BoxPlot::compile(PlotWriterConcept &writer, const Axis &axis) const {
  size_t x = 1;
  for (const BoxPlotData &Box : data)
    Box.compile(writer, axis, BoxStyle(), x++);
}
