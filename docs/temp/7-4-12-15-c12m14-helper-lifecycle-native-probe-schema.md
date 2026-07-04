# C12-M14 Helper Lifecycle Native Probe Schema

## Purpose

This temporary artifact schema records FreeCAD native behavior for `Part.BRepOffsetAPI.MakePipeShell` / `BRepOffsetAPI_MakePipeShellPy` mutable helper methods. It is S2 evidence only: it does not add `cad-core` fixtures, expected files, tests, or C++ implementation authorization. S3 must still decide product-contract and current-mismatch gates.

## Files

- Probe runner: `docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe.py`
- Probe output: `docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json`
- Runtime baseline: `docs/temp/7-4-12-15-c12m14-helper-lifecycle-freecadcmd-version.txt`

## Top-Level Fields

- `schema_version`: fixed to `c12m14.helper-lifecycle-native-probe.v1`.
- `source_authority`: FreeCAD binding source for the helper lifecycle methods.
- `input_artifact`: probe runner path and checked-in context used by S2.
- `execution_model`: one `FreeCADCmd` process per case, using `-c exec(compile(open(...)))`.
- `freecadcmd`: discovered executable, version, OCCT, LibPack, app home and raw baseline payload.
- `process_failures`: case-level process failures including missing payload, timeout, non-zero exit, crash signal, Qt/neon sandbox startup failure, and native instability strings such as `NCollection_Sequence::ChangeValue`.
- `cases`: case payloads keyed by case id. Each case includes:
  - `case_id`, `scope_ids`, `methods`, `description`.
  - `method_sequence`: ordered operations attempted inside the helper.
  - `input_summary`: spine/profile shape summaries and parameter values.
  - `operations`: return summaries or exception diagnostics for each helper call.
  - `process`: exact command argv, returncode, shell exit, stdout/stderr and tails.
  - `status`: `stable_native_payload`, `stable_native_diagnostic`, `not_collected`, or `blocked_by_environment`.
  - `can_enter_s4`: always `false` in S2; S3 is the implementation gate.
- `oracle_classification`: summary rows mapped to C12-M14 oracle ids.
- `blockers`: native probe blockers, if any.
- `conclusion`: `stable_native_probe_payload_s2_only`, `native_instability_blocker`, `native_probe_blocked`, or `sandbox_runtime_limit`.

## Required Coverage

- `add/isReady/getStatus/build/shape/makeSolid` baseline subset.
- `remove` before add, after add before build, after build, and remove/readd ordering.
- `firstShape/lastShape` in unbuilt, build-failed, and build-success states.
- `generated(profile)` parameters, list/empty result and exception diagnostics.
- `simulate(count)` count, shape-list result, pre/post build and failure diagnostics.
- `remove/readd/simulate/build` combination; any `NCollection_Sequence::ChangeValue` or native crash remains a blocker.

## S4 Rule

No row can enter S4 directly from this file. Stable native payload only allows S3 to evaluate product-contract/current-mismatch gates. Missing `FreeCADCmd`, Qt/neon sandbox failure, timeout, missing payload, non-zero exit, native crash, or `NCollection_Sequence::ChangeValue` forces `can_enter_s4=false` and a blocker row.
