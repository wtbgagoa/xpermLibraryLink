cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
project(xperm-LL LANGUAGES C CXX)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FindWolframLanguage)
include(FindWolframLibrary)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

option(ENABLE_PGO_GENERATE "Generate PGO profile" OFF)
option(ENABLE_PGO_USE "Use PGO profile" OFF)

set(XPERM_OPT_FLAGS
    -O3
    -march=native
    -flto
)

add_library(xperm STATIC src/xperm.cpp)
add_library(NiehoffCanonicalizer STATIC src/NiehoffCanonicalizer.cpp)
add_library(xpermLL SHARED src/LLInterface.cpp)

if(ENABLE_PGO_GENERATE)
    list(APPEND XPERM_OPT_FLAGS -fprofile-generate)
endif()

if(ENABLE_PGO_USE)
    list(APPEND XPERM_OPT_FLAGS -fprofile-use -fprofile-correction)
endif()

foreach(target xperm NiehoffCanonicalizer xpermLL)
 target_compile_features(${target} PUBLIC cxx_std_17)
 target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
 target_compile_options(${target} PRIVATE ${XPERM_OPT_FLAGS})
endforeach()

if(ENABLE_PGO_GENERATE)
    target_link_options(xpermLL PRIVATE -fprofile-generate)
elseif(ENABLE_PGO_USE)
    target_link_options(xpermLL PRIVATE -fprofile-use)
endif()

set_target_properties(xperm NiehoffCanonicalizer PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(xpermLL PRIVATE xperm NiehoffCanonicalizer WolframLibrary::WolframLibrary)

if(WIN32)
    target_link_options(xpermLL PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

install(TARGETS xpermLL LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}" RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
