# C12-M3 Native Provenance Probe Artifact Schema

## Purpose

C12-M3 reuses the C12-M2 native probe harness for the process and runtime envelope, but C12-M2's schema is not sufficient for ProjectOnSurface provenance. C12-M3 artifacts must record the source endpoint, target endpoint, history API call and request-local judgement for each observed provenance row before any current comparison is allowed.

S3 does not collect ProjectOnSurface family expected data, does not run current `cad-core` comparison, and does not treat the C12-M2 `None` history result as final. S4 must re-probe with this schema.

## Harness Decision

- Reuse `docs/temp/6-29-20-12-c12m2-native-probe-harness.py` for `freecadcmd`, argv, stdout/stderr, exit code, runtime metadata and timeout capture.
- C12-M3 probe scripts should pass a C12-M3 summary object through `--expected-summary-json`. That summary object carries the provenance schema below.
- The C12-M2 top-level `conclusion` remains a wrapper classification. C12-M3 classification is stored in `expected_summary.c12m3_classification` and in each `expected_summary.provenance_observations[].classification`.
- A dedicated C12-M3 harness is not required in S3; if S4 needs one, it must preserve this schema and still emit the same artifact fields.

## Artifact Fields

Top-level wrapper fields stay compatible with C12-M2:

- `schema_version`: C12-M2 wrapper value if the reused harness writes the artifact.
- `probe.id`, `probe.family`, `probe.case_id`.
- `source_authority`.
- `input_artifact`.
- `freecadcmd`.
- `command`.
- `process`.
- `exception_classification`.
- `expected_summary`.
- `request_local`.
- `current_comparison_path`.
- `conclusion`.

The C12-M3 provenance payload under `expected_summary` must contain:

- `schema_version`: fixed to `c12m3.native-provenance-summary.v1`.
- `artifact_kind`: `project_on_surface_native_provenance_probe`.
- `c12m3_classification`: one value from the frozen C12-M3 classification set.
- `source_authority`: exact FreeCAD source files, classes/functions and short supporting field/function names.
- `input_fixture_or_probe`: fixture, probe script and case id used by S4.
- `runtime_summary`: FreeCAD version, OCCT version, LibPack fields and run mode copied from the wrapper.
- `result_shape_summary`: shape type, topology counts, bbox/area/volume/length when useful, marked as non-provenance evidence.
- `provenance_observations`: array of row-level observations. Each row must contain:
  - `observation_id`.
  - `axis`.
  - `source_endpoint`: source object/property/subname/shape role, or `null` with a blocker reason.
  - `target_endpoint`: target object/result role/subname/shape role, or `null` with a blocker reason.
  - `history_api_name`: for example `TopoShapePy.getElementHistory`, `TopoShapePy.mapShapes`, `TopoShapePy.mapSubElement`, `TopoShapeExpansion.mapSubElement`, `MapperHistory`, or `ElementMap`.
  - `history_return_summary`: return type, count, short `repr`, `None` result, error class/message, or visible source-to-target mapping summary.
  - `request_local_judgement`: whether the observation is derived from one FreeCADCmd request result/intermediate shape without GUI session, cross-request native document, persistent ElementMap/NamedShape cache or full BREP transport.
  - `classification`: one value from the frozen C12-M3 classification set.
  - `current_comparison_path`: S5 input path when expected-ready, or the exact reason comparison is blocked.
- `diagnostics`: stable diagnostics that explain blocker or expected-ready status.
- `non_evidence`: explicit list when bbox, output order, topology counts, fixture names or current `cad-core` ledger fields were observed but rejected as provenance.

## Frozen C12-M3 Classifications

- `native_provenance_expected_ready`: S4 observed source-backed native provenance from a source endpoint to a target endpoint through a named history API. This is the only S4 class that can feed S5 current comparison.
- `current_covered`: S5-only. S4 produced expected-ready provenance and current `cad-core` already matches it.
- `backend_gap_candidate`: S5/S6-only. S4 produced expected-ready provenance and S5 found a stable current mismatch. This only authorizes a later implementation package.
- `native_hidden_retained`: Native ProjectOnSurface or TopoShape history APIs still return `None`, empty, or non-source-backed data; no current comparison is allowed.
- `collector_bug`: The probe script, invocation or artifact construction is wrong; fix collector before interpreting semantics.
- `product_boundary_rejected`: Evidence requires GUI/Workbench state, cross-request native document state, persistent TopoDS/NamedShape/ElementMap cache, full BREP transport, bbox/order/count/fixture guessing, or current-ledger inference.
- `sandbox_runtime_limit`: Current runtime cannot start or complete FreeCADCmd, for example Qt/processor/startup/timeout limits.

## S4 Artifact Naming

S4 should write one or more of these artifacts:

- `docs/temp/<M-D-HH-mm>-c12m3-s4-project-on-surface-edge-wire-native-provenance-probe-output.json`
- `docs/temp/<M-D-HH-mm>-c12m3-s4-project-on-surface-face-rebuild-native-provenance-probe-output.json`
- `docs/temp/<M-D-HH-mm>-c12m3-s4-project-on-surface-all-compound-native-provenance-probe-output.json`
- `docs/temp/<M-D-HH-mm>-c12m3-s4-project-on-surface-invalid-diagnostic-probe-output.json`
- `docs/temp/<M-D-HH-mm>-c12m3-s4-project-on-surface-api-observability-probe-output.json`

## Passing Standard

An S4 artifact passes schema review only when every admitted probe row has source endpoint, target endpoint, history API name, history return summary, request-local judgement, classification and current comparison path. `native_provenance_expected_ready` is valid only when the history API output itself exposes source-backed provenance; output order, bbox, topology count, fixture name and current `cad-core` fields do not qualify.

Rows classified as `native_hidden_retained`, `collector_bug`, `product_boundary_rejected` or `sandbox_runtime_limit` must keep `current_comparison_path` blocked. S5 may compare only rows classified `native_provenance_expected_ready`.
