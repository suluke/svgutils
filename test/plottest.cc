#include "svgutils/plotlib.h"
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
    BoxPlotData data;
    boxplot->addData(std::move(data));
  }
  // PlotWriter is a template and therefore not usable as argument for
  // Graph's `compile` interface method. Wrapping it in a PlotWriterModel
  // turns it into a valid instance of PlotWriterConcept.
  PlotWriterModel<PlotWriter<svg::SVGFormattedWriter>> writer(std::cout);
  // When the graph is fully set up, we can 'compile' it, i.e. write it
  // out to a writer object.
  g.compile(writer);
  return 0;
}
