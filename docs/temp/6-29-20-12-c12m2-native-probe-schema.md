# C12-M2 Native Probe Artifact Schema

## Purpose

S3 freezes the common artifact shape for C12-M2 S4/S5 native probes. The schema is only for native oracle collection evidence; it does not publish support, update expected fixtures, compare current `cad-core`, or authorize implementation.

## Artifact Fields

Every S4/S5 native probe artifact must contain these fields:

- `schema_version`: fixed to `c12m2.native-probe-artifact.v1`.
- `probe.id`, `probe.family`, `probe.case_id`.
- `source_authority`: exact FreeCAD source file plus class/function or helper entry.
- `input_artifact`: fixture, probe script, or request-local DTO used as input.
- `freecadcmd.path`, `freecadcmd.version`, `freecadcmd.occt_version`, `freecadcmd.libpack`, `freecadcmd.libpack_version`.
- `command`: argv used to run the native probe.
- `process.stdout`, `process.stderr`, `process.exit_code`.
- `exception_classification`: one of the fixed C12-M2 classifications below.
- `expected_summary`: stable summary or blocker summary. For geometry, include shape kind, subshape counts, bbox, volume/area, or diagnostics.
- `request_local.judgement`: whether the input and output fit CAD Core request-local product boundaries.
- `current_comparison_path`: where S6 can compare current `cad-core`, or why comparison is not allowed.
- `conclusion`: same controlled vocabulary as `exception_classification`.

## Classifications

- `expected_ready`: stable native expected exists. For the S3 runtime baseline only, this means FreeCADCmd version/OCCT/LibPack are readable; it does not publish a family geometry expected.
- `native_probe_blocked`: FreeCAD native command cannot produce a stable payload.
- `helper_blocked`: FreeCAD helper or wrapper lifecycle blocks stable expected collection.
- `native_hidden`: native API keeps the evidence hidden or unexportable.
- `sandbox_runtime_limit`: current Codex/runtime environment cannot start or complete FreeCADCmd, such as Qt/processor/startup/timeout limits.
- `collector_bug`: C12-M2 probe harness or collector logic is wrong and must be fixed before interpreting the result.
- `product_boundary_rejected`: behavior depends on GUI/session/persistent native state and is outside CAD Core request-local scope.
- `retained_no_expected`: only historical crash, timeout, notCollected, helper noise, or probe-only evidence remains.

## Harness

Use `docs/temp/6-29-20-12-c12m2-native-probe-harness.py` to wrap S4/S5 probe scripts. The harness captures command, stdout/stderr, exit code, FreeCAD version, OCCT/LibPack, expected summary, request-local judgement, comparison path and conclusion into the schema above.

S3 baseline command:

```bash
python3 docs/temp/6-29-20-12-c12m2-native-probe-harness.py --baseline --probe-id C12M2-PROBE-S3-RUNTIME --family global_runtime_baseline --case-id freecadcmd_version_occt_libpack --source-authority "cad-core/tools/collect_freecad_expected.py::run_via_freecadcmd plus FreeCAD.ConfigDump and Part.OCC_VERSION" --input-artifact "docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-probe.py" --request-local not_applicable_runtime_baseline --request-local-notes "S3 collects runtime metadata only; S4/S5 decide family request-local expected boundaries." --comparison-path "S6 only compares current cad-core after S4/S5 produce a family expected_ready artifact." --out docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json
```

S4/S5 family probes should use the same harness with `--freecad-script`, family-specific `--expected-summary-json`, and an explicit `--conclusion` when the native script proves `helper_blocked`, `native_hidden`, `product_boundary_rejected`, `collector_bug`, or `retained_no_expected`.

FreeCADCmd `-c` should execute probe files via `exec(compile(open(...).read(), ...))`; S3 found that long raw multiline `-c` strings can fail before emitting stdout. The harness therefore keeps the outer `-c` short and runs file-backed probe scripts.
