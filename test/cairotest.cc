#include "svgcairo/svg_cairo.h"

int main() {
  using namespace svg;
  CairoSVGWriter writer("test.pdf", 300., 200.);
  // We can use containers to pass attributes as well
  std::vector<SVGAttribute> rectAttrs(
      {width("100%"), height("100%"), fill("red")});
  // We support attribute initialization with const char*, integers and
  // floats
  writer
      .rect(rectAttrs)
      // The following currently forces a newline in the formatted output
      .enter()
      .leave()
      .circle(cx(150), cy(100), r(80), fill("green"))
      .text(x(150), y(125), font_size(60), text_anchor("middle"), fill("white"))
      .enter()
      .content("SVG")
      .leave();
  return 0;
}
