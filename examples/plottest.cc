#include "svgplotlib/plotlib.h"
#include "svgutils/svg_formatted_writer.h"
#include <iostream>

using namespace plots;

int main() {
  Graph g(300, 200);
  // A graph consists of one or more axes
  Axis *axis = g.addAxis(std::make_unique<Axis>());
  // Axes host one or more plots.
  // Axes own their plots and graphs own their axes. That's the reason
  // why we pass those using std::unique_ptr.
  BoxPlot *boxplot = axis->addPlot(std::make_unique<BoxPlot>("test"));
  {
    // BoxPlotData is also owned by their parent plot. However, since the
    // struct has no virtual members (i.e. it's not supposed to be subclassed
    // - it's even marked `final`!) we avoid heap allocations and pass by
    // value
    std::array<BoxPlotData, 3> data = {
        {{0.7, 1, 2, 3, 4}, {2.5, 3, 4, 4.5, 5}, {0.5, 1, 2, 2.7, 3.5}}};
    for (const auto &box : data)
      boxplot->addData(box);
  }
  // PlotWriter is a template and therefore not usable as argument for
  // Graph's `compile` interface method. Wrapping it in a PlotWriterModel
  // turns it into a valid instance of PlotWriterConcept.
  PlotWriterModel<PlotWriter<svg::SVGFormattedWriter>> writer(std::cout);
  // When the graph is fully set up, we can 'compile' it, i.e. write it
  // out to a writer object.
  g.addCSSRule({"width", "300px"});
  g.addCSSRule({"height", "200px"});
  g.compile(writer);
  writer.finish();
  return 0;
}
