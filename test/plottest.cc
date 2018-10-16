#include "svgutils/plotlib.h"
#include <iostream>

using namespace svg;

int main() {
  Graph g(300, 200);
  auto axisAlloc = std::make_unique<Axis>();
  Axis *axis = axisAlloc.get();
  g.addAxis(std::move(axisAlloc));
  std::ignore = axis;
  return 0;
}
