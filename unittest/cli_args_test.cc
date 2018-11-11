#include "svgutils/cli_args.h"
#include "gtest/gtest.h"
#include <array>

using namespace ::svg;
namespace fs = ::std::filesystem;

static const char *TOOLNAME = "cli_args_test";
static const char *TOOLDESC = "Unittests for the cli_args library";

/// Simple test to check for string list parsing support
TEST(CliArgsTest, StringList) {
  cl::list<std::string> Strings(cl::name("s"), cl::init({"Inputs"}));
  std::array args{"", "-s", "a", "b", "c"};
  cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data());
  ASSERT_EQ(Strings->size(), 3);
  EXPECT_EQ(Strings[0], "a");
  EXPECT_EQ(Strings[1], "b");
  EXPECT_EQ(Strings[2], "c");
}

/// Ensure automatic unregistration of destructed cl::opt/cl::list
TEST(CliArgsTest, AutoUnregister) {
  // Dual registration __should__ be an error if no unregistration happens
  { cl::opt<unsigned> Value(cl::name("u"), cl::init(0)); }
  cl::opt<unsigned> Value(cl::name("u"), cl::init(1));
  std::array args{"", "-u", "2"};
  cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data());
  EXPECT_EQ(Value, 2);
}

/// Test support for cl::option_end and continuing with another cli
/// parser configuration afterwards
TEST(CliArgsTest, MultiParse) {
  std::array args{"", "--version", "cmd", "path"};
  int offset = 1;
  {
    cl::opt<bool> PrintVersion(cl::name("version"), cl::init(false));
    cl::opt<std::string> Command(cl::meta("command"), cl::option_end());
    offset +=
        cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data(), offset)
            .getNumArgsRead();
    EXPECT_TRUE(PrintVersion);
    EXPECT_EQ(*Command, "cmd");
  }
  ASSERT_EQ(offset, 3);
  cl::opt<fs::path> Outpath(cl::meta("Outpath"), cl::required());
  cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data(), offset);
  EXPECT_EQ(Outpath, "path");
}
