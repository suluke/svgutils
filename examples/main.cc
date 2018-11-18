#include "svgplotlib/plotlib.h"
#include "svgutils/svg_formatted_writer.h"
#include "svgutils/js_writer.h"
#include <string>

template <typename WriterTy> void testSVG() {
  using namespace svg;
  // There is a special helper class if you want to extend the basic
  // functionality of the provided svg writer classes without losing the
  // ability to switch between unformatted and formatted writing.
  // Note that the struct below is itself templated over this function's
  // WriterTy type.
  // If you know which writer you want to extend then there is no need
  // to use this class.
  // Rule of thumb: If you want to change the output formatting, inherit
  // from SVGWriterBase (or any subclass that may already exist)
  // If you want to add functionality, inherit from ExtendableWriter
  struct ExtendedWriter : public ExtendableWriter<ExtendedWriter> {
    ExtendedWriter(svg::outstream_t &os)
        : ExtendableWriter<ExtendedWriter>(
              std::make_unique<WriterModel<WriterTy>>(os)) {}
    ExtendedWriter &helloWorld() {
      // We can use containers to pass attributes as well
      std::array<SVGAttribute, 3> rectAttrs(
          {width("100%"), height("100%"), fill("red")});
      // We support attribute initialization with const char*, integers and
      // floats
      auto &self = static_cast<ExtendableWriter<ExtendedWriter> &>(*this);
      self.rect(rectAttrs)
          // The following currently forces a newline in the formatted output
          .enter()
          .leave()
          .circle(cx(150), cy(100), r(80), fill("green"))
          .text(x(150), y(125), font_size(60), text_anchor("middle"),
                fill("white"))
          .enter()
          .content("SVG")
          .leave();
      return *this;
    }
  };
  ExtendedWriter svg(std::cout);
  svg.svg(xmlns(), baseProfile(), version(), width(300), height(200))
      .enter()
      .helloWorld();
  // different writers are easily combinable if implemented correctly
  plots::PlotWriter<WriterTy> plot(std::cout);
  svg.continueAs(plot)
      .grid(0, 0, 300, 200, 10, 10, stroke("black"), stroke_dasharray("1 1"))
      .finish();
  std::cout << "\n" << std::endl;
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    std::cerr << "USAGE: test [raw|formatted|js]" << std::endl;
    return 1;
  }
  std::string command = argv[1];
  if (command == "raw")
    testSVG<svg::SVGWriter>();
  else if (command == "formatted")
    testSVG<svg::SVGFormattedWriter>();
  else if (command == "js")
    testSVG<svg::SVGJSWriter>();
  else {
    std::cerr << "Unknown format specified: " << command << std::endl;
    return 1;
  }

  return 0;
}
