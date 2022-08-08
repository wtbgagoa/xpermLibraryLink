#Copyright 2019-2020 Wolfram Research Inc.

#Permission is hereby granted, free of charge, to any person obtaining a copy of
#this software and associated documentation files (the "Software"), to deal in
#the Software without restriction, including without limitation the rights to
#use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
#the Software, and to permit persons to whom the Software is furnished to do so,
#subject to the following conditions:

#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.

#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
#FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
#COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


# FindWolframLibrary.cmake
#
# Finds the Wolfram Library header files
#
# This will define the following variables
#
#    WolframLibrary_FOUND
#    WolframLibrary_INCLUDE_DIRS
#    WolframLibrary_VERSION
#
# and the following imported target
#
#     WolframLibrary::WolframLibrary
#
# You can specify custom location to search for Wolfram Library either by specifying WOLFRAM_LIBRARY_PATH explicitly,
# or if that variable is not set, by providing WolframLanguage_INSTALL_DIR variable with a path to a WolframLanguage installation.
#
# Author: Rafal Chojna - rafalc@wolfram.com


if(WOLFRAM_LIBRARY_PATH)
	set(_WOLFLIB_LIBRARY_PATH "${WOLFRAM_LIBRARY_PATH}")
else()
	set(_WOLFLIB_LIBRARY_PATH "$ENV{WOLFRAM_LIBRARY_PATH}")
endif()

if(NOT _WOLFLIB_LIBRARY_PATH AND WolframLanguage_INSTALL_DIR)
	set(_WOLFLIB_LIBRARY_PATH "${WolframLanguage_INSTALL_DIR}/SystemFiles/IncludeFiles/C")
endif()

if(NOT _WOLFLIB_LIBRARY_PATH AND Mathematica_INSTALL_DIR)
	set(_WOLFLIB_LIBRARY_PATH "${Mathematica_INSTALL_DIR}/SystemFiles/IncludeFiles/C")
endif()

if (_WOLFLIB_LIBRARY_PATH)
	set(_WOLFLIB_SEARCH_OPTS NO_DEFAULT_PATH)
else()
	set(_WOLFLIB_SEARCH_OPTS)
endif()

find_path(WolframLibrary_INCLUDE_DIR
	NAMES WolframLibrary.h
	PATHS "${_WOLFLIB_LIBRARY_PATH}"
	${_WOLFLIB_SEARCH_OPTS}
	DOC "Path to the WolframLibrary.h and other header files from Wolfram Library"
)

if(WolframLibrary_FOUND)
	file(STRINGS "${WolframLibrary_INCLUDE_DIR}/WolframLibrary.h" _WOLFLIB_HEADER_CONTENTS REGEX "#define WolframLibraryVersion ")
	string(REGEX REPLACE ".*#define WolframLibraryVersion ([0-9]+).*" "\\1" WolframLibrary_VERSION "${_WOLFLIB_HEADER_CONTENTS}")

	mark_as_advanced(WolframLibrary_FOUND WolframLibrary_INCLUDE_DIR WolframLibrary_VERSION)
	set(WolframLibrary_INCLUDE_DIRS ${WolframLibrary_INCLUDE_DIR})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WolframLibrary
	REQUIRED_VARS WolframLibrary_INCLUDE_DIR
	VERSION_VAR WolframLibrary_VERSION
)

if(WolframLibrary_FOUND AND NOT TARGET WolframLibrary::WolframLibrary)
	add_library(WolframLibrary::WolframLibrary INTERFACE IMPORTED)
	set_target_properties(WolframLibrary::WolframLibrary PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${WolframLibrary_INCLUDE_DIR}"
)
endif()
