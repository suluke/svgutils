#include "svgutils/cli_args.h"
#include "svgutils/svg_formatted_writer.h"
#include "svgutils/svg_reader_writer.h"

#include <filesystem>
#include <fstream>
#include <variant>

using namespace svg;
namespace fs = std::filesystem;

static cl::opt<fs::path> Infile(cl::meta("Input"), cl::required());
static cl::opt<fs::path> Outfile(cl::name("o"), cl::init("-"));
static cl::opt<bool> Verbose(cl::name("v"), cl::init(false));

static const char *TOOLNAME = "svgfmt";
static const char *TOOLDESC = "Format SVG documents";

int main(int argc, const char **argv) {
  cl::ParseArgs(TOOLNAME, TOOLDESC, argc, argv);
  if (!fs::exists(Infile)) {
    std::cerr << "Input file does not exist" << std::endl;
    return 1;
  }
  std::fstream in(Infile->c_str(), in.in);
  std::optional<std::fstream> out_storage;
  std::ostream *out = nullptr;
  if (*Outfile == "-")
    out = &std::cout;
  else {
    out_storage = std::fstream(Outfile->c_str(), std::fstream::out);
    out = &*out_storage;
  }

  SVGReaderWriter<SVGFormattedWriter> Reader(*out);
  if (auto err = Reader.parse(in)) {
    std::cerr << "An error occurred:\n" << *err << std::endl;
    return 1;
  }
  return 0;
}
