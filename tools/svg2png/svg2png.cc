#include "svgutils/cli_args.h"
#include "svgutils/svg_reader_writer.h"
#include "svgcairo/svg_cairo.h"

#include <filesystem>
#include <fstream>

using namespace svg;
namespace fs = std::filesystem;

static cl::opt<fs::path> Infile(cl::meta("Input"), cl::required());
static cl::opt<fs::path> Outfile(cl::name("o"), cl::required());
static cl::opt<bool> Verbose(cl::name("v"), cl::init(false));
static cl::opt<unsigned> Width(cl::name("w"), cl::init(0));
static cl::opt<unsigned> Height(cl::name("h"), cl::init(0));
static cl::opt<unsigned> DefaultWidth(cl::name("W"), cl::init(300));
static cl::opt<unsigned> DefaultHeight(cl::name("H"), cl::init(200));

static const char *TOOLNAME = "svg2png";
static const char *TOOLDESC = "Convert SVG documents to PNG images";

int main(int argc, const char **argv) {
  cl::ParseArgs(TOOLNAME, TOOLDESC, argc, argv);
  if (!fs::exists(Infile)) {
    std::cerr << "Input file does not exist" << std::endl;
    return 1;
  }
  std::fstream in(Infile->c_str(), in.in);
  if (!Width && !Height) {
    SVGReaderWriter<CairoSVGWriter> Reader(Outfile, CairoSVGWriter::PNG);
    CairoSVGWriter &cairo = Reader.getWriter();
    cairo.setDefaultWidth(DefaultWidth);
    cairo.setDefaultHeight(DefaultHeight);
    Reader.parse(in);
  } else {
    if (!Width || !Height) {
      std::cerr << "PNG dimension zero or not set" << std::endl;
      return 1;
    }
    SVGReaderWriter<CairoSVGWriter> Reader(Outfile, CairoSVGWriter::PNG, Width, Height);
    Reader.parse(in);
  }
  return 0;
}
