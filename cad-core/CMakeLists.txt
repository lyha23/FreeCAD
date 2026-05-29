cmake_minimum_required(VERSION 3.20)

project(cad_core_mvp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCASCADE CONFIG REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)

find_path(NLOHMANN_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  PATHS /opt/homebrew/include /usr/local/include
)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp was not found")
endif()

add_library(cad-core-lib STATIC
  src/document/model.cpp
  src/features/body.cpp
  src/features/feature_base.cpp
  src/features/feature_extrude.cpp
  src/features/feature_executor.cpp
  src/features/pad.cpp
  src/features/pocket.cpp
  src/features/sketch_object.cpp
  src/geometry/shape_exporter.cpp
  src/graph/recompute_plan.cpp
  src/runtime/compute_context.cpp
  src/runtime/diagnostics.cpp
  src/runtime/feature_registry.cpp
  src/runtime/io.cpp
  src/runtime/recompute.cpp
  src/topo/subshape_map.cpp
)

set_target_properties(cad-core-lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(cad-core-lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${OpenCASCADE_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
)

target_link_directories(cad-core-lib PUBLIC
  ${OpenCASCADE_LIBRARY_DIR}
)

target_link_libraries(cad-core-lib PUBLIC
  TKernel
  TKMath
  TKG2d
  TKG3d
  TKGeomBase
  TKBRep
  TKGeomAlgo
  TKTopAlgo
  TKPrim
  TKMesh
  TKBO
)

add_library(cad_core_ffi SHARED
  src/adapters/c_api/c_api.cpp
)

target_link_libraries(cad_core_ffi PRIVATE cad-core-lib)
target_include_directories(cad_core_ffi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(cad-core
  src/adapters/cli/cli.cpp
  src/adapters/cli/main.cpp
)

target_link_libraries(cad-core PRIVATE cad-core-lib)

if(APPLE)
  set_target_properties(cad-core PROPERTIES
    BUILD_RPATH "${OpenCASCADE_LIBRARY_DIR}"
    INSTALL_RPATH "${OpenCASCADE_LIBRARY_DIR}"
  )
  set_target_properties(cad_core_ffi PROPERTIES
    BUILD_RPATH "${OpenCASCADE_LIBRARY_DIR}"
    INSTALL_RPATH "${OpenCASCADE_LIBRARY_DIR}"
    INSTALL_NAME_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    BUILD_WITH_INSTALL_NAME_DIR ON
  )
endif()
