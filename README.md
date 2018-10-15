# svgutils
Conveniently write SVG documents from C++

## Demo
Build this project as you would with any other CMake project:
```
mkdir build
cd build
cmake ..
make
```
You will now find a `test` executable in your build directory
It supports three different output styles: `raw`, `formatted` and `js`.
E.g. you can do
```
./test js > ../test/test.js
```
Afterwards, you can test the generated js code by opening the `index.html` file in the `test` directory with your browser.

## Features
I'm currently too lazy to list all the features in this document.
A good pointer is to look at the source code of [`test/main.cc`](test/main.cc).
