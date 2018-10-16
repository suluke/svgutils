#include "svgutils/plotlib.h"
#include <algorithm>
#include <limits>

using namespace svg;
using namespace plots;

void Axis::compile(PlotWriterConcept &writer, double width,
                   double height) const {}
Point<2> Axis::project(Point<2> p) const {
  p.x(p.x());
  p.y(p.y());
  return p;
}

void Graph::compile(PlotWriterConcept &writer) const {
  writer.svg(svg::width(width), svg::height(height));
  for (const auto &Axis : Axes)
    Axis->compile(writer, width, height);
}

template <typename container_t>
static std::string ConcatStyles(const container_t &styles) {
  std::stringstream ss;
  for (const CSSRule &Rule : styles)
    ss << Rule.property << "=\"" << Rule.value << "\";";
  return ss.str();
}

void BoxPlotData::compile(PlotWriterConcept &writer, const BoxStyle &style, size_t x) const {}

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
void BoxPlot::compile(PlotWriterConcept &writer) const {
  size_t x = 1;
  for (const BoxPlotData &Box : data)
    Box.compile(writer, BoxStyle(), x++);
}
