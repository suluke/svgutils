#ifndef SVGUTILS_PLOTLIB_H
#define SVGUTILS_PLOTLIB_H

#include "plotlib_core.h"

namespace plots {

struct BoxStyle final {
  double boxWidth = 0.5;
  double topWhiskerWidth = 0.5;
  double bottomWhiskerWidth = 0.5;

  std::vector<CSSRule> topWhiskerStyles;
  std::vector<CSSRule> bottomWhiskerStyles;
  std::vector<CSSRule> upperQuartileStyles;
  std::vector<CSSRule> lowerQuartileStyles;
  std::vector<CSSRule> medianStyles;
  std::vector<CSSRule> boxLeftStyles;
  std::vector<CSSRule> boxRightStyles;
};

struct BoxPlotData final {
  BoxPlotData() = default;
  BoxPlotData(const BoxPlotData &) = default;
  BoxPlotData &operator=(const BoxPlotData &) = default;
  BoxPlotData(BoxPlotData &&) = default;
  BoxPlotData &operator=(BoxPlotData &&) = default;
  BoxPlotData(double bottom, double lower, double median, double upper,
              double top)
      : bottomWhisker(bottom), lowerQuartile(lower), median(median),
        upperQuartile(upper), topWhisker(top) {}

  double bottomWhisker;
  double lowerQuartile;
  double median;
  double upperQuartile;
  double topWhisker;

  void compile(PlotWriterConcept &writer, const Axis &axis,
               const BoxStyle &style, size_t x) const;
};

struct BoxPlot : public Plot {
  BoxPlot(std::string name) : Plot(std::move(name)) {}

  double getMinX() const override;
  double getMaxX() const override;
  double getMinY() const override;
  double getMaxY() const override;
  void renderPreview(PlotWriterConcept &writer) const override;
  void compile(PlotWriterConcept &writer, const Axis &axis) const override;

  void addData(BoxPlotData data) { this->data.emplace_back(std::move(data)); }

private:
  double plotPadding;
  std::vector<BoxPlotData> data;
};

} // namespace plots
#endif // SVGUTILS_PLOTLIB_H
