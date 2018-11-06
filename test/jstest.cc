#include "svgutils/js_writer.h"
#include <iostream>
// In this test we build a little js library to create svg documents.
// We use a nifty feature of the SVGJSWriter for that.

/// This struct helps us to create the required glue code for our library's
/// functions.
struct JSFunc {
  JSFunc(const char *signature, svg::outstream_t &os) : os(&os) {
    os << "function " << signature << " {\n  const rootTags = [];\n";
  }
  ~JSFunc() { (*os) << "  return rootTags[0];\n}\n"; }
  svg::outstream_t *os;
};

using namespace svg;
void createMakeSVG() {
  // We exploit the fact that attribute values are written as template
  // strings by SVGJSWriter
  JSFunc func("makeSVG(w, h)", std::cout);
  SVGJSWriter writer(std::cout, "rootTags");
  writer.svg(width("${w}"), height("${h}")).finish();
}

void createMakeRect() {
  JSFunc func("makeRect(x, y, w, h)", std::cout);
  SVGJSWriter writer(std::cout, "rootTags");
  writer.rect(x("${x}"), y("${y}"), width("${w}"), height("${h}")).finish();
}

int main() {
  createMakeSVG();
  createMakeRect();
  return 0;
}
