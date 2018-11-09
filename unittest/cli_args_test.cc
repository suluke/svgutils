#include "svgutils/cli_args.h"
#include "gtest/gtest.h"
#include <array>

using namespace ::svg;
namespace fs = ::std::filesystem;


//~ static cl::opt<bool> OneBool(cl::name("b"), cl::init(true));
//~ static cl::opt<fs::path> OnePath(cl::name("p"), cl::required());

static const char *TOOLNAME = "cli_args_test";
static const char *TOOLDESC = "Unittests for the cli_args library";

TEST(CliArgsTest, StringList) {
  cl::list<std::string> Strings(cl::name("s"), cl::init({"Inputs"}));
  std::array args{"", "-s", "a", "b", "c"};
  cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data());
  ASSERT_EQ(Strings->size(), 3);
  EXPECT_EQ(Strings[0], "a");
  EXPECT_EQ(Strings[1], "b");
  EXPECT_EQ(Strings[2], "c");
}
TEST(CliArgsTest, AutoUnregister) {
  // Dual registration __should__ be an error if no unregistration happens
  {
    cl::opt<unsigned> Value(cl::name("u"), cl::init(0));
  }
  cl::opt<unsigned> Value(cl::name("u"), cl::init(1));
  std::array args{"", "-u", "2"};
  cl::ParseArgs(TOOLNAME, TOOLDESC, args.size(), args.data());
  EXPECT_EQ(Value, 2);
}
