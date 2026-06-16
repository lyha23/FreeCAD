cmake_minimum_required(VERSION 3.20)

project(cad_core_mvp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAD_CORE_OCCT_COMPONENTS
  FoundationClasses
  ModelingData
  ModelingAlgorithms
  DataExchange
)

find_package(OpenCASCADE CONFIG QUIET COMPONENTS ${CAD_CORE_OCCT_COMPONENTS})

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
  )
  if(TARGET TKDE)
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKDE)
  endif()
  if(TARGET TKDESTL)
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKDESTL)
  else()
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKSTL)
  endif()
  if(TARGET TKDEIGES)
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKDEIGES)
  else()
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKIGES)
  endif()
  if(TARGET TKDESTEP)
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKDESTEP)
  else()
    list(APPEND CAD_CORE_OCCT_LIBRARIES TKSTEPBase TKSTEPAttr TKSTEP209 TKSTEP)
  endif()
else()
  find_path(CAD_CORE_OCCT_INCLUDE_DIR
    NAMES Standard_Version.hxx
    PATHS
      /usr/include/opencascade
      /usr/local/include/opencascade
      /opt/homebrew/include/opencascade
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
    TKSTEPBase
    TKSTEPAttr
    TKSTEP209
    TKSTEP
    TKIGES
    TKSTL
  )

  set(CAD_CORE_OCCT_LIBRARIES)
  set(CAD_CORE_OCCT_LIBRARY_DIRS)
  foreach(CAD_CORE_OCCT_LIBRARY_NAME IN LISTS CAD_CORE_OCCT_LIBRARY_NAMES)
    find_library(CAD_CORE_OCCT_${CAD_CORE_OCCT_LIBRARY_NAME}_LIBRARY
      NAMES ${CAD_CORE_OCCT_LIBRARY_NAME}
      PATHS
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
        /opt/homebrew/lib
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

  list(REMOVE_DUPLICATES CAD_CORE_OCCT_LIBRARY_DIRS)
  set(CAD_CORE_OCCT_INCLUDE_DIRS ${CAD_CORE_OCCT_INCLUDE_DIR})
endif()

if(NOT CAD_CORE_OCCT_INCLUDE_DIRS)
  message(FATAL_ERROR "OpenCASCADE headers were not found")
endif()

find_path(NLOHMANN_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdParty/json/single_include
    /opt/homebrew/include
    /usr/local/include
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

set(CAD_CORE_ONDSEL_SOLVER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/3rdParty/OndselSolver")
if(EXISTS "${CAD_CORE_ONDSEL_SOLVER_DIR}/CMakeLists.txt")
  set(CAD_CORE_HAS_ONDSEL_SOLVER ON)
  set(ONDSELSOLVER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(ONDSELSOLVER_BUILD_EXE OFF CACHE BOOL "" FORCE)
  add_subdirectory("${CAD_CORE_ONDSEL_SOLVER_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ondsel_solver" EXCLUDE_FROM_ALL)
else()
  set(CAD_CORE_HAS_ONDSEL_SOLVER OFF)
endif()

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
  src/part/part_import.cpp
  src/part/part_offset.cpp
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
  src/part_design/feature_chamfer.cpp
  src/part_design/feature_dress_up.cpp
  src/part_design/feature_draft.cpp
  src/part_design/feature_extrude.cpp
  src/part_design/feature_fillet.cpp
  src/part_design/feature_hole.cpp
  src/part_design/feature_linear_pattern.cpp
  src/part_design/feature_mirrored.cpp
  src/part_design/feature_multi_transform.cpp
  src/part_design/feature_pad.cpp
  src/part_design/feature_pocket.cpp
  src/part_design/feature_polar_pattern.cpp
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
  ${CAD_CORE_OCCT_INCLUDE_DIRS}
  ${NLOHMANN_JSON_INCLUDE_DIR}
)

target_include_directories(cad-core-lib PRIVATE
  ${ZSTD_INCLUDE_DIR}
)

target_compile_definitions(cad-core-lib PRIVATE
  CAD_CORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(CAD_CORE_HAS_ONDSEL_SOLVER)
  target_compile_definitions(cad-core-lib PRIVATE CAD_CORE_HAS_ONDSEL_SOLVER=1)
  target_include_directories(cad-core-lib PRIVATE "${CAD_CORE_ONDSEL_SOLVER_DIR}")
  target_link_libraries(cad-core-lib PUBLIC OndselSolver)
else()
  target_compile_definitions(cad-core-lib PRIVATE CAD_CORE_HAS_ONDSEL_SOLVER=0)
endif()

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
target_include_directories(cad_core_ffi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
  if(CAD_CORE_HAS_ONDSEL_SOLVER)
    set(CAD_CORE_ONDSEL_RPATH "$<TARGET_FILE_DIR:OndselSolver>")
  else()
    set(CAD_CORE_ONDSEL_RPATH "")
  endif()
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
