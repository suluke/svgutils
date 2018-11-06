#include "svgplotlib/plotlib.h"
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

template <typename plots_t>
static void updateBounds(AxisStyle &style, const plots_t &plots) {
  double &minX = style.minX;
  double &maxX = style.maxX;
  double &minY = style.minY;
  double &maxY = style.maxY;
  constexpr double infty = std::numeric_limits<double>::infinity();
  minX = infty;
  maxX = -infty;
  minY = infty;
  maxY = -infty;
  for (const auto &plot : plots) {
    assert(plot && "Found nullptr in plots");
    minX = std::min(minX, plot->getMinX());
    maxX = std::max(maxX, plot->getMaxX());
    minY = std::min(minY, plot->getMinY());
    maxY = std::max(maxY, plot->getMaxY());
  }
}

std::string FontInfo::getFontStyle() const {
  std::stringstream ss;
  ss << "font-family:" << font << ";font-size: " << fontSize << ";";
  return ss.str();
}
double FontInfo::getWidth(std::string_view text, bool multiLine) const {
  return text.size() * fontSize;
}
double FontInfo::getHeight(std::string_view text, bool multiLine) const {
  return fontSize;
}
void FontInfo::placeText(const char *text, PlotWriterConcept &writer,
                         Point<2> position, Point<2> anchor,
                         bool multiLine) const {
  if (multiLine)
    std::cerr << "Warning: Multi-line text placing not implemented\n";
  double w = getWidth(text, multiLine);
  double h = getHeight(text, multiLine);
  using namespace svg;
  writer.text(x(position.x() + anchor.x() * w),
              y(position.y() + anchor.y() * h));
  writer.enter();
  writer.content(text);
  writer.leave();
}

AxisStyle &Axis::getStyle() {
  if (!style)
    prepareStyle();
  return *style;
}
AxisStyle &Axis::prepareStyle() {
  style = AxisStyle();
  updateBounds(*style, plots);
  return *style;
}

void Axis::compile(PlotWriterConcept &writer, const Graph &graph, double width,
                   double height) {
  this->width = width;
  this->height = height;
  // initialize style if necessary
  if (!style)
    prepareStyle();
  const AxisStyle &style = getStyle();
  using namespace svg;
  std::string trans;
  {
    std::stringstream ss;
    ss << "translate(0 " << height << ") scale(1, -1)";
    trans = ss.str();
  }
  writer.g(x(0), y(0), transform(trans.c_str()));
  writer.enter();
  double xAxisY = 0.;
  double yAxisX = 0.;
  if (style.type == AxisStyle::OUTER) {
    xAxisY = style.minY;
    yAxisX = style.minX;
  }
  Point<2> xLeft(project({style.minX, xAxisY}));
  Point<2> xRight(project({style.maxX, xAxisY}));
  writer.line(x1(xLeft.x()), y1(xLeft.y()), x2(xRight.x()), y2(xRight.y()),
              svg::style("stroke: black;stroke-width: 0.5;"));
  Point<2> yTop(project({yAxisX, style.maxY}));
  Point<2> yBottom(project({yAxisX, style.minY}));
  writer.line(x1(yBottom.x()), y1(yBottom.y()), x2(yTop.x()), y2(yTop.y()),
              svg::style("stroke: black;stroke-width: 0.5;"));
  for (const auto &Plot : plots)
    Plot->compile(writer, *this);
  writer.leave();
}

Point<2> Axis::project(Point<2> p) const {
  const AxisStyle &style = getStyle();
  double minX = style.minX;
  double maxX = style.maxX;
  double minY = style.minY;
  double maxY = style.maxY;
  double xRange = maxX - minX;
  double yRange = maxY - minY;
  {
    maxY += 0.1 * yRange;
    if (minY < 0)
      minY -= 0.1 * yRange;
    yRange = maxY - minY;
  }
  p.x((p.x() - minX) * width / xRange + translation.x());
  p.y((p.y() - minY) * height / yRange + translation.y());
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
    Axis->compile(writer, *this, width, height);
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
