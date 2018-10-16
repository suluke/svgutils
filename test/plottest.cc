#include "svgutils/plotlib.h"
#include <iostream>

using namespace plots;

int main() {
  Graph g(300, 200);
  auto axisAlloc = std::make_unique<Axis>();
  Axis *axis = axisAlloc.get();
  auto boxplotsAlloc = std::make_unique<BoxPlot>("test");
  BoxPlot *boxplot = boxplotsAlloc.get();
  boxplot->addData(BoxPlotData());
  axis->addPlot(std::move(boxplotsAlloc));
  g.addAxis(std::move(axisAlloc));
  std::ignore = axis;
  return 0;
}
