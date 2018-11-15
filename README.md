# svgutils
Conveniently work with SVG documents from C++

## Project Overview
This project suffers from severe feature creep.
I usually implement any idea I have that seems to be reasonably simple to realize.
So far, I've added the following components to this repository (in no particular order):
* `svg_writer.h`: The original svg writer implementation that started this all.
  Every writer exposes functions to create tags with arbitray attributes which the user can `enter` and `leave` or set the `content` of.
* `cli_args.h`: A header-only, declarative command line argument parsing library.
* `svg_reader_writer.h`: A hand-written SVG parser that immediately dispatches to arbitrary svg writer implementations.
* `svgcairo`: An svg writer implementation that translates API calls into Cairo calls.
  This allows creation of many different graphics formats using only established svg functionalities.
* `svgplotlib`: This should eventually allow users to define plots that are rendered using any svg writer.
  Development is currently on hold as long as the other features need to become more robust.
* `tools` and `utils`: Various executables to perform specific svg-releated tasks like formatting or conversion to other formats.

## Build Instructions
This project requires a C++17-capable compiler.
Build this project as you would with any other CMake project.
E.g. on Linux:
```
git clone https://github.com/suluke/svgutils.git
cd svgutils
git submodule update --init
mkdir build
cd build
cmake ..
make
```
**NOTE** for Windows users:   
I'd be super happy if this project was usable on Windows.
However, I don't use this OS myself, so nothing has been tested and I imagine there are already some obstacles.
E.g., Freetype, Fontconfig and Cairo are far less likely to be installed already (at least in a way that CMake can find them).
Furthermore, I have no idea how far VS compilers are w.r.t. C++17 support.
I'm thankful for any MRs addressing these issues and willing to offer support however I can.

## Usage Instructions
After building the project the build directory will roughly look like this:
```
.
├── examples
│   ├── cairotest      - Create an example PDF file
│   ├── jstest         - Create a JavaScript file that dynamically creates an SVG in the browser
│   ├── plottest       - Create an example box plot using plotlib
│   └── svgtest        - Output a test SVG document using different writers (raw, formatted, js)
├── libsvgutils.a      - The library powering this project
├── tools
│   ├── svg2pdf
│   │   └── svg2pdf    - Convert SVG files to PDF
│   ├── svg2png
│   │   └── svg2png    - Convert SVG files to PNG
│   └── svgfmt
│       └── svgfmt     - Format SVG documents using SVGFormattedWriter
├── unittest
│   └── cli_args_test  - Unittest for the cli_args header
└── utils
    └── svg2cxx
        └── svg2cxx    - Generate C++(17) header files from SVG documents
```
Since this project is still in early development don't expect these tools to work with arbitrary SVG documents.
However, the following demonstrates nicely what can already be done:
```
[build] $ ./examples/svgtest raw > test.svg
[build] $ ./tools/svg2png/svg2png test.svg -o test.png
[build] $ ./tools/svg2pdf/svg2pdf test.svg -o test.pdf
```
This writes an example SVG document to `test.svg`.
Then this SVG is converted to PDF and PNG format using the corresponding tools.

## Contributing
Merge Requests are very welcome.
If your contributions can be unit tested, please add a test to the `unittests` directory.
I'm currently working on another tool to make integration tests and regression tests more easy.
It will roughly resemble [llvm-lit](http://llvm.org/docs/CommandGuide/lit.html) in case you ever worked with it.
After this tool has landed, MRs will be expected to make use of this tool as well.

## License
This project is MIT licensed.
See [LICENSE](LICENSE) for details
