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

void Axis::updateBounds(double width, double height) {
  this->width = width;
  this->height = height;
  minX = infty;
  maxX = -infty;
  minY = infty;
  maxY = -infty;
  for (const auto &plot : plots) {
    assert(plot && "Trying to add nullptr to plots");
    minX = std::min(minX, plot->getMinX());
    maxX = std::max(maxX, plot->getMaxX());
    minY = std::min(minY, plot->getMinY());
    maxY = std::max(maxY, plot->getMaxY());
  }
}

void Axis::compile(PlotWriterConcept &writer, double width, double height) {
  using namespace svg;
  updateBounds(width, height);
  std::string trans;
  {
    std::stringstream ss;
    ss << "translate(0 " << height << ") scale(1, -1)";
    trans = ss.str();
  }
  writer.g(x(0), y(0), transform(trans.c_str()));
  writer.enter();
  for (const auto &Plot : plots)
    Plot->compile(writer, *this);
  writer.leave();
}

Point<2> Axis::project(Point<2> p) const {
  double minX = this->minX;
  double maxX = this->maxX;
  double minY = this->minY;
  double maxY = this->maxY;
  double xRange = maxX - minX;
  double yRange = maxY - minY;
  {
    maxY += 0.1 * yRange;
    if (minY < 0)
      minY -= 0.1 * yRange;
    yRange = maxY - minY;
  }
  p.x((p.x() - minX) * width / xRange);
  p.y((p.y() - minY) * height / yRange);
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
              svg::style("stroke: black;stroke-width: 0.5;"));
}

static void writeVLine(PlotWriterConcept &writer, const Axis &axis, double x,
                       double y1, double y2) {
  Point<2> Top = axis.project(Point<2>(x, y1));
  Point<2> Bottom = axis.project(Point<2>(x, y2));
  writer.line(x1(Top.x()), svg::y1(Top.y()), x2(Bottom.x()),
              svg::y2(Bottom.y()),
              svg::style("stroke: black;stroke-width: 0.5;"));
}

static void writeRect(PlotWriterConcept &writer, const Axis &axis, double x,
                      double y, double width, double height) {
  using namespace svg;
  Point<2> BottomLeft = axis.project(Point<2>(x, y));
  Point<2> TopRight = axis.project(Point<2>(x + width, y + height));
  writer.rect(svg::x(BottomLeft.x() - width / 2), svg::y(BottomLeft.y()),
              svg::width(TopRight.x() - BottomLeft.x()),
              svg::height(TopRight.y() - BottomLeft.y()),
              style("fill: white;stroke: black;stroke-width: 0.5;"));
}

void BoxPlotData::compile(PlotWriterConcept &writer, const Axis &axis,
                          const BoxStyle &style, size_t x) const {
  writeHLine(writer, axis, x, topWhisker, style.topWhiskerWidth);
  writeVLine(writer, axis, x, bottomWhisker, upperQuartile);
  writeRect(writer, axis, x - style.boxWidth / 2, lowerQuartile, style.boxWidth,
            median - lowerQuartile);
  writeRect(writer, axis, x - style.boxWidth / 2, median, style.boxWidth,
            upperQuartile - median);
  writeVLine(writer, axis, x, upperQuartile, topWhisker);
  writeHLine(writer, axis, x, bottomWhisker, style.bottomWhiskerWidth);
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
