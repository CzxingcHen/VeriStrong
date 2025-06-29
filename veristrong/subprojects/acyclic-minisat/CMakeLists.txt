cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(acyclic-minisat)

set(CMAKE_CXX_STANDARD 20)

#--------------------------------------------------------------------------------------------------
# Configurable options:

option(STATIC_BINARIES "Link binaries statically." ON)
option(USE_SORELEASE   "Use SORELEASE in shared library filename." ON)

#--------------------------------------------------------------------------------------------------
# Library version:

set(MINISAT_SOMAJOR   2)
set(MINISAT_SOMINOR   1)
set(MINISAT_SORELEASE 0)

# Compute VERSION and SOVERSION:
if (USE_SORELEASE)
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR}.${MINISAT_SORELEASE})
else()
  set(MINISAT_VERSION ${MINISAT_SOMAJOR}.${MINISAT_SOMINOR})
endif()
set(MINISAT_SOVERSION ${MINISAT_SOMAJOR})

#--------------------------------------------------------------------------------------------------
# Dependencies:

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIR})
include_directories(${minisat_SOURCE_DIR})

find_package(fmt)

#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
# add_compile_options(-g -pg)
add_compile_options(-Ofast)

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISAT_LIB_SOURCES
    minisat/api/acyclic_minisat.cc
    minisat/utils/Options.cc
    minisat/utils/System.cc
    minisat/utils/Monitor.cc
    minisat/core/Solver.cc
    minisat/core/AcyclicSolver.cc
    minisat/core/AcyclicSolverHelper.cc
    minisat/core/ICDGraph.cc
    minisat/core/Logger.cc
    minisat/simp/SimpSolver.cc)

add_library(minisat-lib-static STATIC ${MINISAT_LIB_SOURCES})
add_library(minisat-lib-shared SHARED ${MINISAT_LIB_SOURCES})

target_link_libraries(minisat-lib-shared ${ZLIB_LIBRARY})
target_link_libraries(minisat-lib-static ${ZLIB_LIBRARY})

target_link_libraries(minisat-lib-shared fmt::fmt)
target_link_libraries(minisat-lib-static fmt::fmt)

add_executable(minisat_core minisat/core/Main.cc)
add_executable(minisat_simp minisat/simp/Main.cc)

if(STATIC_BINARIES)
  target_link_libraries(minisat_core minisat-lib-static)
  target_link_libraries(minisat_simp minisat-lib-static)
else()
  target_link_libraries(minisat_core minisat-lib-shared)
  target_link_libraries(minisat_simp minisat-lib-shared)
endif()

set_target_properties(minisat-lib-static PROPERTIES OUTPUT_NAME "acyclic-minisat")
set_target_properties(minisat-lib-shared
  PROPERTIES
    OUTPUT_NAME "acyclic-minisat" 
    VERSION ${MINISAT_VERSION}
    SOVERSION ${MINISAT_SOVERSION})

set_target_properties(minisat_simp       PROPERTIES OUTPUT_NAME "acyclic-minisat")

#--------------------------------------------------------------------------------------------------
# Installation targets:

install(TARGETS minisat-lib-static minisat-lib-shared minisat_core minisat_simp 
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY minisat/mtl minisat/utils minisat/core minisat/simp
        DESTINATION include/minisat
        FILES_MATCHING PATTERN "*.h")
