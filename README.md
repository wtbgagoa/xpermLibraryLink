# xpermLibraryLink
This is a fork of xperm C-sources used in the [xAct](http://www.xact.es/xPerm/index.html) project. The purpose of this fork is to
replace the [WSTP](https://www.wolfram.com/wstp/) connection used by the original xperm C-source by a newer and faster
[LibraryLink](https://reference.wolfram.com/language/guide/LibraryLink.html) connection. This is a shared library that is loaded
on-demand when required.


# Prerequisites
* A working installation of [xAct](http://www.xact.es/).
* cmake >= 3.0 (only for building from source).
* A C\C++ compiler (only for building from source).

# Precompiled libraries
If you just want a ready to use compiled library you can simply download it from the `binaries`
folder of this repository (choose the library suitable for your platform). You should then copy the library to one
of the directories that is in the `$LibraryPath` variable (this is a Mathematica variable that you can
check from within a Mathematica session).


# General build and installation instructions
## Linux and MacOS
This follows the standard `cmake` procedure and as a result a shared library is built.
We explain the build procedure for Unix operating systems (it should work for other operating systems by doing
suitable changes).

* Change to the folder `xpermLibraryLink` and inside it create a build directory:
```
$cd xpermLibraryLink
$mkdir build
```
* Change to the build directory and use `cmake`
```
$cd build
$cmake -DCMAKE_INSTALL_PREFIX=your_install_dir ..
```
`your_install_dir` must be one of the directories contained in the variable `$LibraryPath`
(this is a Mathematica variable that you can check from within your
[xAct](http://www.xact.es/xPerm/index.html) session).

* If you have several *Mathematica* installations you can select one of them by using the following
`cmake`switch:
```
$cmake -DWolframLanguage_INSTALL_DIR=/usr/local/Wolfram/Mathematica/13.1 -DCMAKE_INSTALL_PREFIX=your_install_dir ..
```
Here we selected the `Mathematica 13.1` installation directory. If you wish to select another one please change
accordingly.

* If `cmake` reports no errors then you can build and install without changing directory:
```
$make
$make install
```
* If the previous step succeeds then you can test your installation using the notebook
[xpermLibraryLinkSample.nb](./xpermLibraryLinkSample.nb) that you can find in the root
folder of this project.

# Windows
We explain the compilation procedure using the development tools included in
[Visual Studio community edition](https://visualstudio.microsoft.com/vs/community/).
When [installing Visual Studio](https://learn.microsoft.com/en-us/visualstudio/install/install-visual-studio?view=vs-2022) you
need to choose in step 4 the "Desktop & Mobile workloads" (that brings the C\C++ compilers) and in step 5 (Individual Components)
you need to tick manually CMake. Since we are going to build the xperm library using the system terminal we need to tell the shell
interpreter where the cmake executable is. To that end please go to Windows "Advanced system settings" and click on the button
"Environment Variables". There, you need to look for the `Path` variable and add the route where the `cmake` executable was installed.
This should be

```
C:\Program Files\microsoft Visual Studio\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin

```
After this you can start the build procedure:

* Open a terminal (Windows PowerShell) change to the project directory `xpermLibraryLink`, create the `build` directory
and change to it

```
PS C:\Users\username\xpermLibraryLink> mkdir build

PS C:\Users\username\xpermLibraryLink> cd build

```
Next use `cmake` in the following way

```
PS C:\Users\username\xpermLibraryLink\build> cmake -DCMAKE_INSTALL_PREFIX=C:\Users\username\AppData\Roaming\Mathematica\SystemFiles\LibraryResources\Windows-x86-64 ..

```

Here, we told `cmake` to install the library in one of the directories stored in the variable
`$LibraryPath` using the `CMAKE_INSTALL_PREFIX` switch. After this we can compile and install the library:


```
PS C:\Users\username\xpermLibraryLink\build> cmake --build .

PS C:\Users\username\xpermLibraryLink\build> cmake --build . --target install

```





