cmake_minimum_required(VERSION 3.16)

project(cmk
    DESCRIPTION "Reverse Cuthill-McKee renumbering of gmsh meshes"
    LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Boost.Graph is header only
find_package(Boost 1.66 REQUIRED)

# gmsh SDK (gmsh.h + libgmsh); set GMSH_DIR if gmsh is installed elsewhere
set(GMSH_DIR "" CACHE PATH "Root directory of the gmsh SDK")
find_path(GMSH_INCLUDE_DIR gmsh.h HINTS ${GMSH_DIR} PATH_SUFFIXES include)
find_library(GMSH_LIBRARY gmsh HINTS ${GMSH_DIR} PATH_SUFFIXES lib lib64)
if(NOT GMSH_INCLUDE_DIR OR NOT GMSH_LIBRARY)
    message(FATAL_ERROR "gmsh SDK not found: set GMSH_DIR (e.g. -DGMSH_DIR=/opt/gmsh)")
endif()
message(STATUS "gmsh: ${GMSH_LIBRARY}")

add_executable(cmk
    main.cc
    reverse_cmk.cc
    update_labelling.cc
    msh22_writer.cc)

target_compile_features(cmk PRIVATE cxx_std_17)
set_target_properties(cmk PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(cmk PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>)
target_include_directories(cmk PRIVATE ${GMSH_INCLUDE_DIR})
target_link_libraries(cmk PRIVATE Boost::boost ${GMSH_LIBRARY})

install(TARGETS cmk RUNTIME DESTINATION bin)
