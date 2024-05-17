cmake_minimum_required(VERSION 3.0)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
project(xperm-LL LANGUAGES C CXX)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FindWolframLanguage)
include(FindWSTP)
include(FindWolframLibrary)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


#=============================================
#== xperm Target LibraryLink shared library ========
#=============================================

add_library(xperm STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/xperm.cpp)

add_library(xpermLL SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/LLInterface.cpp)

#=============================================
#===== Linking xperm to WSTP and LibraryLink
#=============================================


target_link_libraries(xpermLL WSTP::WSTP WolframLibrary::WolframLibrary xperm)

#=============================================
#===== Install targets  ========
#=============================================
install(TARGETS xpermLL DESTINATION ${CMAKE_INSTALL_PREFIX})
