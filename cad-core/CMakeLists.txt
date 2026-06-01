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

find_path(ZSTD_INCLUDE_DIR
  NAMES zstd.h
  PATHS /opt/homebrew/include /opt/homebrew/opt/zstd/include /usr/local/include
)

find_library(ZSTD_LIBRARY
  NAMES zstd
  PATHS /opt/homebrew/lib /opt/homebrew/opt/zstd/lib /usr/local/lib
)

if(NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "libzstd was not found")
endif()

add_library(cad-core-lib STATIC
  src/document/model.cpp
  src/features/body.cpp
  src/features/datum_coordinate_system.cpp
  src/features/datum_line.cpp
  src/features/datum_plane.cpp
  src/features/datum_point.cpp
  src/features/dress_up.cpp
  src/features/feature_base.cpp
  src/features/feature_extrude.cpp
  src/features/feature_executor.cpp
  src/features/hole.cpp
  src/features/link.cpp
  src/features/mesh.cpp
  src/features/part.cpp
  src/features/part_boolean.cpp
  src/features/pad.cpp
  src/features/pocket.cpp
  src/features/sketch_object.cpp
  src/features/transformed.cpp
  src/geometry/brep_snapshot.cpp
  src/geometry/extrusion_helper.cpp
  src/geometry/face_maker.cpp
  src/geometry/placement.cpp
  src/geometry/refine_model.cpp
  src/geometry/shape_exporter.cpp
  src/geometry/sketch_internal_builder.cpp
  src/geometry/wire_joiner.cpp
  src/graph/recompute_plan.cpp
  src/runtime/compute_context.cpp
  src/runtime/diagnostics.cpp
  src/runtime/feature_registry.cpp
  src/runtime/io.cpp
  src/runtime/recompute.cpp
  src/topo/element_map.cpp
  src/topo/named_shape.cpp
  src/topo/reference_matcher.cpp
  src/topo/subshape_map.cpp
)

set_target_properties(cad-core-lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(cad-core-lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${OpenCASCADE_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
)

target_include_directories(cad-core-lib PRIVATE
  ${ZSTD_INCLUDE_DIR}
)

target_compile_definitions(cad-core-lib PRIVATE
  CAD_CORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_directories(cad-core-lib PUBLIC
  ${OpenCASCADE_LIBRARY_DIR}
)

target_link_libraries(cad-core-lib PUBLIC
  ${ZSTD_LIBRARY}
  TKernel
  TKMath
  TKG2d
  TKG3d
  TKGeomBase
  TKBRep
  TKGeomAlgo
  TKTopAlgo
  TKPrim
  TKOffset
  TKShHealing
  TKMesh
  TKBO
  TKDE
  TKDESTL
  TKDEIGES
  TKDESTEP
  TKXSBase
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
