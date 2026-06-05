# CAD Core

This directory contains a standalone, no-Qt CAD Core library and adapters backed
by real OCCT geometry calls. It extracts FreeCAD-style document, Sketcher, Part,
PartDesign, topology, and recompute semantics into a request-local C++ core.

Build it first:

```bash
cmake -S . -B build
cmake --build build
```

Run the acceptance command from this directory:

```bash
./cad-core recompute fixtures/mvp/rect-pad.json --output out/rect-pad.result.json
```

The core accepts a neutral JSON document, recomputes the requested
FreeCAD-style object graph, returns mesh and subshape JSON directly in the
recompute result, and emits stable diagnostics for unsupported or invalid
inputs.

The shell entry point `./cad-core` only delegates to `build/cad-core`; the CAD
implementation is C++ and uses OCCT for wire/face/prism construction, meshing,
bounding boxes, volume, and subshape traversal.

## Module Layout

```text
include/cad_core/
  app/             Document, DocumentObject, properties, links, element maps
  base/            shared value types such as placement
  graph/           dependency analysis and recompute plan
  runtime/         diagnostics, ComputeContext, registry, recompute loop
  sketcher/        SketchObject geometry, constraints, externals, operations
  part/            PartFeature, TopoShape, FaceMaker, WireJoiner, shape helpers
  part_design/     Body, Pad, Pocket, dress-up, pattern, transformed features
  mesh/            mesh import feature support
  assembly/        Assembly object, link, joint-group placeholders
  adapters/        CLI and C ABI adapter surfaces

src/
  app/
  base/
  graph/
  runtime/
  sketcher/
  part/
  part_design/
  mesh/
  assembly/
  adapters/c_api/
  adapters/cli/
```

`cad-core-lib` is the core library target. `cad-core` is only the CLI adapter
linked on top of that library, and `cad_core_ffi` exposes the C ABI used by the
Rust web server.

New semantic declarations should land in the FreeCAD-aligned module directories
above.
