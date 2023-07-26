# ZBinaryIO
[![Build Status](https://travis-ci.org/pawREP/ZBinaryIO.svg?branch=master)](https://travis-ci.org/pawREP/ZBinaryIO)
[![codecov](https://codecov.io/gh/pawREP/ZBinaryReader/branch/master/graph/badge.svg)](https://codecov.io/gh/pawREP/ZBinaryReader) 
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/882591c88b0b45128142c94b47de7e22)](https://www.codacy.com/manual/pawREP/ZBinaryReader?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=pawREP/ZBinaryReader&amp;utm_campaign=Badge_Grade)

ZBinaryIO is a modern, header-only, cross-platform C++17 binary reader/writer library.

The library provides binary read and write interfaces for files and buffers. Additional data sources and sinks can be supported via implementations of the [`ISource`](https://github.com/pawREP/ZBinaryReader/blob/18666af7d1b2ca64c9f95910e518a5acb1970fa1/include/ZBinaryReader.hpp#L22) and [`ISink`](https://github.com/pawREP/ZBinaryReader/blob/18666af7d1b2ca64c9f95910e518a5acb1970fa1/include/ZBinaryWriter.hpp#L22) interfaces. Sources and sinks can also easily extended using the mixin pattern. See [`CoverageTrackingSource`](https://github.com/pawREP/ZBinaryReader/blob/18666af7d1b2ca64c9f95910e518a5acb1970fa1/include/ZBinaryReader.hpp#L75) for an example of such an extension.

# Basic Usage 

```cpp
using namespace ZBio;

// Reader
ZBinaryReader br("example.bin");

// Read fundamental types
int intLe = br.read<int>();
int intBe = br.read<int, Endianness::BE>();

//Align read head
br.align<0x10>();

// Read strings
std::string str0 = br.readCString(); //Read null-terminated string
std::string str1 = br.readString<4>(); //Read fixed size strings

//Read arrays
std::vector<int> ints(10);
br.read(ints.data(). ints.size())

//Read trivially copyable types
struct TrivialStruct{int a; float b; double c}
auto s = br.read<TrivialStruct>();

br.release(); // Release reader resources.

// Writer 
ZBinaryWriter bw("example.bin");

bw.write(3); // Write fundamental types
bw.write<long long, Endianness::BE>(5); // Write fundamental types, explicit type and endianness

bw.writeString("Test"); // Write string
bw.writeCString("Test"); // Write null-terminated string

// Write trivially copyable type
TrivialStruct ts{1, 2, 3.25};
bw.write(ts); 

// Write arrays
std::vector<int> wints(10);
br.write(wints.data(). wints.size())

// Release writer resources
bw.release();

```

# CI
ZBinaryIO is continuously compiled and tested on:
-  gcc 9.3.0
-  clang 9.0.0
-  MSVC 2019 ver. 16.5 (1925) 
