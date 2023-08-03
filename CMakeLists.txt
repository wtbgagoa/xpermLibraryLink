cmake_minimum_required(VERSION 3.0)

project(xperm-LL LANGUAGES C CXX)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FindWolframLanguage)
include(FindWSTP)
include(FindWolframLibrary)

#=============================================
#== xperm Target LibraryLink shared library ========
#=============================================

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(xperm SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/xperm.cpp)

#=============================================
#===== Linking xperm to WSTP and LibraryLink
#=============================================


target_link_libraries(xperm WSTP::WSTP WolframLibrary::WolframLibrary)

#=============================================
#===== Install targets  ========
#=============================================
install(TARGETS xperm DESTINATION ${CMAKE_INSTALL_PREFIX})
