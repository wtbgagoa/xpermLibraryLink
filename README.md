# xpermLibraryLink
This is a fork of xperm C-sources used in the [xAct](http://www.xact.es/xPerm/index.html) project. The purpose of this fork is to
replace the [WSTP](https://www.wolfram.com/wstp/) connection used by the original xperm C-source by a newer and faster
[LibraryLink](https://reference.wolfram.com/language/guide/LibraryLink.html) connection. This is a shared library that is loaded
on-demand when required. An anproved algorithm by Ben Niehoff (https://github.com/bniehoff/tensor-canonicalizer) is now added.


# Prerequisites
* A working installation of [xAct](http://www.xact.es/).
* cmake >= 3.0 (only for building from source).
* A C\C++ compiler (only for building from source).

# Precompiled libraries
If you just want a ready to use compiled library you can simply download it from the `binaries`
folder of this repository (choose the library suitable for your platform). You should then copy the library to one
of the directories that is in the `$LibraryPath` variable (this is a Mathematica variable that you can
check from within a Mathematica session).




