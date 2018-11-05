#include "cli_args.h"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace svg;
namespace fs = std::filesystem;

static cl::opt<fs::path> Infile{cl::meta("Input"), cl::required()};
static cl::opt<fs::path> Outfile(cl::name("o"), cl::required());

static const char *TOOLNAME = "svg2cxx";
static const char *TOOLDESC = "svg2cxx";

int main(int argc, const char **argv) {
  cl::ParseArgs(TOOLNAME, TOOLDESC, argc, argv);
  if (!fs::exists(Infile)) {
    std::cerr << "Input file does not exist" << std::endl;
    return 1;
  }
  std::fstream in(Infile->c_str(), in.in);
  std::fstream out(Outfile->c_str(), out.out);
  out << "#ifndef SVG\n"
      << "#define SVG(CONTENT)\n"
      << "#endif\n"
      << "SVG(u8R\"<\"<\"<(\n"
      << in.rdbuf() << ")<\"<\"<\")\n"
      << "#undef SVG\n";
  return 0;
}
