#include "svgcairo/svg_cairo.h"
#include "svgplotlib/plotlib_core.h"

void testCairo() {
  using namespace svg;
  using WriterTy = CairoSVGWriter;
  WriterTy writer("test.pdf", WriterTy::PDF);
  // We can use containers to pass attributes as well
  std::vector<SVGAttribute> rectAttrs(
      {width("100%"), height("100%"), fill("red")});
  // We support attribute initialization with const char*, integers and
  // floats
  writer.svg(xmlns(), baseProfile(), version(), width(300), height(200))
      ->enter()
      ->rect(rectAttrs)
      // The following currently forces a newline in the formatted output
      ->enter()
      ->leave()
      ->circle(cx(150), cy(100), r(80), fill("green"))
      ->text(x(150), y(125), font_size(60), text_anchor("middle"), fill("white"))
      ->enter()
      ->content("SVG")
      ->leave();
  plots::PlotWriter<WriterTy> plot(std::move(writer));
  plot.grid(0, 0, 300, 200, 10, 10, stroke("black"), stroke_dasharray("1 1"))->finish();
}

int main() {
  testCairo();
  return 0;
}
