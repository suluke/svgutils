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

static const char *TOOLNAME = "svg2pdf";
static const char *TOOLDESC = "Convert SVG documents to PDF files";

int main(int argc, const char **argv) {
  cl::ParseArgs(TOOLNAME, TOOLDESC, argc, argv);
  if (!fs::exists(Infile)) {
    std::cerr << "Input file does not exist" << std::endl;
    return 1;
  }
  std::fstream in(Infile->c_str(), in.in);
  SVGReaderWriter<CairoSVGWriter> Reader(Outfile, 300, 200);
  Reader.parse(in);
  return 0;
}
