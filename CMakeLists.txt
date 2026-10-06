cmake_minimum_required(VERSION 3.5)

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

option(ENABLE_PGO_GENERATE "Generate PGO profile" OFF)
option(ENABLE_PGO_USE "Use PGO profile" OFF)

set(XPERM_OPT_FLAGS
    -O3
    -march=native
    -flto
)

if(ENABLE_PGO_GENERATE)
    list(APPEND XPERM_OPT_FLAGS -fprofile-generate)
endif()

if(ENABLE_PGO_USE)
    list(APPEND XPERM_OPT_FLAGS -fprofile-use -fprofile-correction)
endif()

target_compile_options(xperm PRIVATE ${XPERM_OPT_FLAGS})
target_compile_options(xpermLL PRIVATE ${XPERM_OPT_FLAGS})

if(ENABLE_PGO_GENERATE)
    target_link_options(xpermLL PRIVATE -fprofile-generate)
elseif(ENABLE_PGO_USE)
    target_link_options(xpermLL PRIVATE -fprofile-use)
endif()


#=============================================
#===== Linking xperm to WSTP and LibraryLink
#=============================================


if(WIN32)
    target_link_libraries(xpermLL
        WSTP::WSTP
        xperm
    )
else()
    target_link_libraries(xpermLL
        WSTP::WSTP
        WolframLibrary::WolframLibrary
        xperm
    )
endif()

if(WIN32)
    target_link_options(xpermLL PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()


#target_link_libraries(xpermLL WSTP::WSTP WolframLibrary::WolframLibrary xperm)

#=============================================
#===== Install targets  ========
#=============================================
install(TARGETS xpermLL DESTINATION ${CMAKE_INSTALL_PREFIX})
