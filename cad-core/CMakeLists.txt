cmake_minimum_required(VERSION 3.20)

project(cad_core_mvp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAD_CORE_OCCT_SUPPORTED_VERSIONS 7.8.1 7.9.3)
string(REPLACE ";" " or " CAD_CORE_OCCT_SUPPORTED_VERSIONS_TEXT "${CAD_CORE_OCCT_SUPPORTED_VERSIONS}")

set(CAD_CORE_OCCT_ROOT "" CACHE PATH "Preferred OpenCASCADE/OCCT installation prefix")
if(NOT CAD_CORE_OCCT_ROOT)
  foreach(CAD_CORE_OCCT_CANDIDATE_VERSION IN LISTS CAD_CORE_OCCT_SUPPORTED_VERSIONS)
    set(CAD_CORE_OCCT_CANDIDATE_ROOT "$ENV{HOME}/.local/share/occt-${CAD_CORE_OCCT_CANDIDATE_VERSION}/.pixi/envs/default")
    if(EXISTS "${CAD_CORE_OCCT_CANDIDATE_ROOT}/include/opencascade/Standard_Version.hxx")
      set(CAD_CORE_OCCT_ROOT "${CAD_CORE_OCCT_CANDIDATE_ROOT}" CACHE PATH "Preferred OpenCASCADE/OCCT installation prefix" FORCE)
      break()
    endif()
  endforeach()
endif()

set(CAD_CORE_OCCT_COMPONENTS
  FoundationClasses
  ModelingData
  ModelingAlgorithms
  DataExchange
)

find_package(OpenCASCADE CONFIG QUIET
  PATHS ${CAD_CORE_OCCT_ROOT}
  COMPONENTS ${CAD_CORE_OCCT_COMPONENTS}
)

if(OpenCASCADE_FOUND AND UNIX AND OpenCASCADE_DIR MATCHES "^/mnt/[A-Za-z]/")
  message(STATUS "Ignoring OpenCASCADE package from Windows path: ${OpenCASCADE_DIR}")
  set(OpenCASCADE_FOUND FALSE)
  unset(OpenCASCADE_DIR CACHE)
endif()

if(OpenCASCADE_FOUND)
  set(CAD_CORE_OCCT_INCLUDE_DIRS ${OpenCASCADE_INCLUDE_DIR})
  set(CAD_CORE_OCCT_LIBRARY_DIRS ${OpenCASCADE_LIBRARY_DIR})
  set(CAD_CORE_OCCT_LIBRARIES
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKBRep
    TKGeomAlgo
    TKTopAlgo
    TKPrim
    TKFeat
    TKOffset
    TKShHealing
    TKMesh
    TKBO
    TKFillet
    TKBool
    TKXSBase
    TKDE
    TKDESTL
    TKDEIGES
    TKDESTEP
  )
else()
  find_path(CAD_CORE_OCCT_INCLUDE_DIR
    NAMES Standard_Version.hxx
    PATHS
      ${CAD_CORE_OCCT_ROOT}/include/opencascade
      /usr/include/opencascade
      /usr/local/include/opencascade
      /opt/homebrew/include/opencascade
  )

  set(CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS
    ${CAD_CORE_OCCT_ROOT}/lib
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib
    /opt/homebrew/lib
  )

  set(CAD_CORE_OCCT_LIBRARY_NAMES
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKBRep
    TKGeomAlgo
    TKTopAlgo
    TKPrim
    TKFeat
    TKFillet
    TKBool
    TKOffset
    TKShHealing
    TKMesh
    TKBO
    TKXSBase
  )

  set(CAD_CORE_OCCT_LIBRARIES)
  set(CAD_CORE_OCCT_LIBRARY_DIRS)
  foreach(CAD_CORE_OCCT_LIBRARY_NAME IN LISTS CAD_CORE_OCCT_LIBRARY_NAMES)
    find_library(CAD_CORE_OCCT_${CAD_CORE_OCCT_LIBRARY_NAME}_LIBRARY
      NAMES ${CAD_CORE_OCCT_LIBRARY_NAME}
      PATHS ${CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS}
    )
    if(NOT CAD_CORE_OCCT_${CAD_CORE_OCCT_LIBRARY_NAME}_LIBRARY)
      message(FATAL_ERROR "OpenCASCADE library ${CAD_CORE_OCCT_LIBRARY_NAME} was not found")
    endif()
    list(APPEND CAD_CORE_OCCT_LIBRARIES ${CAD_CORE_OCCT_${CAD_CORE_OCCT_LIBRARY_NAME}_LIBRARY})
    get_filename_component(CAD_CORE_OCCT_LIBRARY_DIR
      "${CAD_CORE_OCCT_${CAD_CORE_OCCT_LIBRARY_NAME}_LIBRARY}"
      DIRECTORY
    )
    list(APPEND CAD_CORE_OCCT_LIBRARY_DIRS ${CAD_CORE_OCCT_LIBRARY_DIR})
  endforeach()

  find_library(CAD_CORE_OCCT_TKDE_LIBRARY NAMES TKDE PATHS ${CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS})
  if(NOT CAD_CORE_OCCT_TKDE_LIBRARY)
    message(FATAL_ERROR "OpenCASCADE library TKDE was not found")
  endif()
  list(APPEND CAD_CORE_OCCT_LIBRARIES ${CAD_CORE_OCCT_TKDE_LIBRARY})

  find_library(CAD_CORE_OCCT_TKDESTL_LIBRARY NAMES TKDESTL PATHS ${CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS})
  if(NOT CAD_CORE_OCCT_TKDESTL_LIBRARY)
    message(FATAL_ERROR "OpenCASCADE library TKDESTL was not found")
  endif()
  list(APPEND CAD_CORE_OCCT_LIBRARIES ${CAD_CORE_OCCT_TKDESTL_LIBRARY})

  find_library(CAD_CORE_OCCT_TKDEIGES_LIBRARY NAMES TKDEIGES PATHS ${CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS})
  if(NOT CAD_CORE_OCCT_TKDEIGES_LIBRARY)
    message(FATAL_ERROR "OpenCASCADE library TKDEIGES was not found")
  endif()
  list(APPEND CAD_CORE_OCCT_LIBRARIES ${CAD_CORE_OCCT_TKDEIGES_LIBRARY})

  find_library(CAD_CORE_OCCT_TKDESTEP_LIBRARY NAMES TKDESTEP PATHS ${CAD_CORE_OCCT_LIBRARY_SEARCH_PATHS})
  if(NOT CAD_CORE_OCCT_TKDESTEP_LIBRARY)
    message(FATAL_ERROR "OpenCASCADE library TKDESTEP was not found")
  endif()
  list(APPEND CAD_CORE_OCCT_LIBRARIES ${CAD_CORE_OCCT_TKDESTEP_LIBRARY})

  list(REMOVE_DUPLICATES CAD_CORE_OCCT_LIBRARY_DIRS)
  set(CAD_CORE_OCCT_INCLUDE_DIRS ${CAD_CORE_OCCT_INCLUDE_DIR})
endif()

if(NOT CAD_CORE_OCCT_INCLUDE_DIRS)
  message(FATAL_ERROR "OpenCASCADE headers were not found")
endif()

find_file(CAD_CORE_OCCT_VERSION_HEADER
  NAMES Standard_Version.hxx
  PATHS ${CAD_CORE_OCCT_INCLUDE_DIRS}
  NO_DEFAULT_PATH
)

if(NOT CAD_CORE_OCCT_VERSION_HEADER)
  message(FATAL_ERROR "OpenCASCADE Standard_Version.hxx was not found")
endif()

file(STRINGS "${CAD_CORE_OCCT_VERSION_HEADER}" CAD_CORE_OCCT_VERSION_LINE
  REGEX "^#define[ \t]+OCC_VERSION_COMPLETE[ \t]+\"[^\"]+\""
)
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" CAD_CORE_OCCT_VERSION "${CAD_CORE_OCCT_VERSION_LINE}")
if(NOT CAD_CORE_OCCT_VERSION IN_LIST CAD_CORE_OCCT_SUPPORTED_VERSIONS)
  message(FATAL_ERROR "CAD Core expected OpenCASCADE ${CAD_CORE_OCCT_SUPPORTED_VERSIONS_TEXT}, found ${CAD_CORE_OCCT_VERSION} at ${CAD_CORE_OCCT_VERSION_HEADER}")
endif()

set(CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS)
if(CAD_CORE_OCCT_ROOT)
  list(APPEND CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS "${CAD_CORE_OCCT_ROOT}/include")
endif()
foreach(CAD_CORE_OCCT_INCLUDE_DIR IN LISTS CAD_CORE_OCCT_INCLUDE_DIRS)
  if(CAD_CORE_OCCT_INCLUDE_DIR MATCHES "/include/opencascade$")
    get_filename_component(CAD_CORE_OCCT_PREFIX_INCLUDE_DIR "${CAD_CORE_OCCT_INCLUDE_DIR}" DIRECTORY)
    list(APPEND CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS "${CAD_CORE_OCCT_PREFIX_INCLUDE_DIR}")
  endif()
endforeach()
list(APPEND CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdParty/json/single_include
  /opt/homebrew/include
  /usr/local/include
)
list(REMOVE_DUPLICATES CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS)

foreach(CAD_CORE_NLOHMANN_JSON_CANDIDATE IN LISTS CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS)
  if(EXISTS "${CAD_CORE_NLOHMANN_JSON_CANDIDATE}/nlohmann/json.hpp")
    set(NLOHMANN_JSON_INCLUDE_DIR "${CAD_CORE_NLOHMANN_JSON_CANDIDATE}" CACHE PATH "nlohmann/json.hpp include directory" FORCE)
    break()
  endif()
endforeach()

find_path(NLOHMANN_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  PATHS ${CAD_CORE_NLOHMANN_JSON_SEARCH_PATHS}
)

if(NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp was not found")
endif()

if(CAD_CORE_OCCT_ROOT
   AND EXISTS "${CAD_CORE_OCCT_ROOT}/include/zstd.h"
   AND EXISTS "${CAD_CORE_OCCT_ROOT}/lib/libzstd.so")
  set(ZSTD_INCLUDE_DIR "${CAD_CORE_OCCT_ROOT}/include" CACHE PATH "zstd include directory" FORCE)
  set(ZSTD_LIBRARY "${CAD_CORE_OCCT_ROOT}/lib/libzstd.so" CACHE FILEPATH "zstd library" FORCE)
else()
  find_path(ZSTD_INCLUDE_DIR
    NAMES zstd.h
    PATHS /opt/homebrew/include /opt/homebrew/opt/zstd/include /usr/local/include
  )

  find_library(ZSTD_LIBRARY
    NAMES zstd
    PATHS /opt/homebrew/lib /opt/homebrew/opt/zstd/lib /usr/local/lib
  )
endif()

if(NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "libzstd was not found")
endif()

set(CAD_CORE_ONDSEL_SOLVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdParty/OndselSolver")
if(NOT EXISTS "${CAD_CORE_ONDSEL_SOLVER_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "CAD Core requires the bundled OndselSolver adapter at ${CAD_CORE_ONDSEL_SOLVER_DIR}")
endif()
set(CAD_CORE_HAS_ONDSEL_SOLVER ON)
set(ONDSELSOLVER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ONDSELSOLVER_BUILD_EXE OFF CACHE BOOL "" FORCE)
add_subdirectory("${CAD_CORE_ONDSEL_SOLVER_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ondsel_solver" EXCLUDE_FROM_ALL)

add_library(cad-core-lib STATIC
  src/assembly/assembly_link.cpp
  src/assembly/assembly_object.cpp
  src/assembly/assembly_utils.cpp
  src/assembly/joint_solver.cpp
  src/assembly/joint_group.cpp
  src/app/copy_on_change.cpp
  src/app/document.cpp
  src/app/document_object.cpp
  src/app/element_map.cpp
  src/app/link.cpp
  src/app/property.cpp
  src/app/property_geo.cpp
  src/app/property_links.cpp
  src/base/placement.cpp
  src/graph/recompute_plan.cpp
  src/mesh/feature_mesh_import.cpp
  src/part/brep_snapshot.cpp
  src/part/extrusion_helper.cpp
  src/part/face_maker.cpp
  src/part/part_boolean.cpp
  src/part/part_compound.cpp
  src/part/part_extrusion.cpp
  src/part/part_feature.cpp
  src/part/part_feature_support.cpp
  src/part/part_filling.cpp
  src/part/part_geomplate.cpp
  src/part/part_geometry_curve.cpp
  src/part/part_import.cpp
  src/part/part_loft.cpp
  src/part/part_offset.cpp
  src/part/part_project_on_surface.cpp
  src/part/part_ruled_surface.cpp
  src/part/part_sweep.cpp
  src/part/part_thickness.cpp
  src/part/primitive_feature.cpp
  src/part/property_topo_shape.cpp
  src/part/refine_model.cpp
  src/part/shape_fix.cpp
  src/part/shape_exporter.cpp
  src/part/topo_shape.cpp
  src/part/topo_shape_expansion.cpp
  src/part/topo_shape_mapper.cpp
  src/part/topo_shape_reference.cpp
  src/part/wire_joiner.cpp
  src/part_design/body.cpp
  src/part_design/datum_coordinate_system.cpp
  src/part_design/datum_line.cpp
  src/part_design/datum_plane.cpp
  src/part_design/datum_point.cpp
  src/part_design/feature_base.cpp
  src/part_design/feature_boolean.cpp
  src/part_design/feature_chamfer.cpp
  src/part_design/feature_dress_up.cpp
  src/part_design/feature_draft.cpp
  src/part_design/feature_extrude.cpp
  src/part_design/feature_fillet.cpp
  src/part_design/feature_groove.cpp
  src/part_design/feature_hole.cpp
  src/part_design/feature_linear_pattern.cpp
  src/part_design/feature_loft.cpp
  src/part_design/feature_mirrored.cpp
  src/part_design/feature_multi_transform.cpp
  src/part_design/feature_pad.cpp
  src/part_design/feature_pipe.cpp
  src/part_design/feature_pocket.cpp
  src/part_design/feature_polar_pattern.cpp
  src/part_design/profile_resolver.cpp
  src/part_design/feature_revolved.cpp
  src/part_design/feature_revolution.cpp
  src/part_design/feature_scaled.cpp
  src/part_design/feature_thickness.cpp
  src/part_design/feature_transformed.cpp
  src/runtime/compute_context.cpp
  src/runtime/diagnostics.cpp
  src/runtime/feature_executor.cpp
  src/runtime/feature_registry.cpp
  src/runtime/io.cpp
  src/runtime/recompute.cpp
  src/sketcher/sketch_object_constraints.cpp
  src/sketcher/sketch_object_external.cpp
  src/sketcher/sketch_object_geometry.cpp
  src/sketcher/sketch_object_operations.cpp
  src/sketcher/sketch_object.cpp
  src/sketcher/sketch_internal_builder.cpp
)

set_target_properties(cad-core-lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(cad-core-lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${CAD_CORE_OCCT_INCLUDE_DIRS}
)

target_include_directories(cad-core-lib PRIVATE
  ${ZSTD_INCLUDE_DIR}
)

target_compile_definitions(cad-core-lib PRIVATE
  CAD_CORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  CAD_CORE_HAS_ONDSEL_SOLVER=1
)
target_include_directories(cad-core-lib PRIVATE "${CAD_CORE_ONDSEL_SOLVER_DIR}")
target_link_libraries(cad-core-lib PUBLIC OndselSolver)

if(CAD_CORE_OCCT_LIBRARY_DIRS)
  target_link_directories(cad-core-lib PUBLIC
    ${CAD_CORE_OCCT_LIBRARY_DIRS}
  )
endif()

target_link_libraries(cad-core-lib PUBLIC
  ${ZSTD_LIBRARY}
  ${CAD_CORE_OCCT_LIBRARIES}
)

add_library(cad_core_ffi SHARED
  src/adapters/c_api/c_api.cpp
)

target_link_libraries(cad_core_ffi PRIVATE cad-core-lib)
target_include_directories(cad_core_ffi
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR}
)

add_executable(cad-core
  src/adapters/cli/cli.cpp
  src/adapters/cli/main.cpp
)

target_link_libraries(cad-core PRIVATE cad-core-lib)

add_executable(cad-core-c3m1-topology-probe
  tests/c3m1_topology_probe.cpp
)

target_link_libraries(cad-core-c3m1-topology-probe PRIVATE cad-core-lib)

if(APPLE)
  set(CAD_CORE_ONDSEL_RPATH "$<TARGET_FILE_DIR:OndselSolver>")
  list(JOIN CAD_CORE_OCCT_LIBRARY_DIRS ";" CAD_CORE_OCCT_RPATH)
  set_target_properties(cad-core PROPERTIES
    BUILD_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
    INSTALL_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
  )
  set_target_properties(cad-core-c3m1-topology-probe PROPERTIES
    BUILD_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
    INSTALL_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
  )
  set_target_properties(cad_core_ffi PROPERTIES
    BUILD_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
    INSTALL_RPATH "${CAD_CORE_OCCT_RPATH};${CAD_CORE_ONDSEL_RPATH}"
    INSTALL_NAME_DIR "${CMAKE_CURRENT_BINARY_DIR}"
    BUILD_WITH_INSTALL_NAME_DIR ON
  )
endif()
