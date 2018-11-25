#include "svgutils/svg_logging_writer.h"
#include "gtest/gtest.h"

#include <sstream>

using namespace ::svg;

TEST(LoggingWriterTest, Simple) {
  std::stringstream ss;
  SVGLoggingWriter<SVGDummyWriter> svg(ss);
  svg.svg()->enter()->text()->enter()->content("Blah")->finish();
  EXPECT_EQ(ss.str(), "svg\nenter\ntext\nenter\ncontent: \"Blah\"\nfinish\n");
}
