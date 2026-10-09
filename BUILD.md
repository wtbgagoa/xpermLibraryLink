# Building on Linux

Configure and build in a fresh directory:

rm -rf build-linux
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j"$(nproc)"

The resulting library is normally:

build-linux/libxpermLL.so

It can be found by the system with

FindLibrary["libxpermLL"]

if you place it in
~/.Wolfram/SystemFiles/LibraryResources/Linux-x86-64


# Cross compiling for Windows on Linux

rm -rf build-win
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j"$(nproc)"

The resulting library is normally:

build-win/libxpermLL.dll

