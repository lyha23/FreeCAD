# CAD Core MVP

This directory contains a standalone, no-Qt MVP CAD Core CLI backed by real
OCCT geometry calls.

Build it first:

```bash
cmake -S . -B build
cmake --build build
```

Run the acceptance command from this directory:

```bash
./cad-core recompute fixtures/mvp/rect-pad.json --output out/rect-pad.result.json
```

The MVP accepts a neutral JSON document, recomputes a minimal
`Sketcher::SketchObject -> PartDesign::Pad -> PartDesign::Body` chain, returns
mesh and subshape JSON directly in the recompute result, and emits stable
diagnostics for unsupported or invalid inputs.

The shell entry point `./cad-core` only delegates to `build/cad-core`; the CAD
implementation is C++ and uses OCCT for wire/face/prism construction, meshing,
bounding boxes, volume, and subshape traversal.

## Module Layout

```text
include/cad_core/
  document/        neutral Document, DocumentObject, link parsing
  graph/           dependency analysis and recompute plan
  runtime/         diagnostics, ComputeContext, registry, recompute loop
  features/        SketchObject, Body, Pad executors
  geometry/        OCCT bbox, volume, mesh, kernel metadata
  topo/            subshape map export
  adapters/        CLI surface

src/
  document/
  graph/
  runtime/
  features/
  geometry/
  topo/
  adapters/cli/
```

`cad-core-lib` is the core library target. `cad-core` is only the CLI adapter
linked on top of that library, and `cad_core_ffi` exposes the C ABI used by the
Rust web server.
