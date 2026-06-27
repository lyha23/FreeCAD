# C9-M1 S3 markerPlacement 与 offsetPlc 复审

## 目标

复审 `handleOneSideOfJoint()` marker placement 与 `offsetPlc` 边界，决定 non-identity bundled `offsetPlc`、non-AssemblyLink subshape primitive frame 是否需要 oracle、实现或保持 non-goal。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`
- `src/Mod/Assembly/App/AssemblyObject.h::AssemblyObject::PartData::offsetPlc`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::jointParts()`
- `src/Mod/Assembly/App/AssemblyLink.cpp`

## 必须复核

- FreeCAD marker placement 是 object-global 到 part-local，而不是只在 linked object local frame 里猜测。
- `offsetPlc` 是 bundled parts 内部 offset，不允许跨请求缓存或 frontend 补猜测。
- current cad-core `joint_solver.cpp` 对 AssemblyLink identity-offset、non-linear edge、non-planar face 和 mixed marker 的覆盖范围。
- `cad-core/fixtures/c3m6/expected/*` 是否已有 non-identity marker chain expected；若有，判断是否已被 current tests 覆盖。
- 如果 native expected 不足，写明 collector / probe 基线和不采集的原因，不把缺证据写成 backendGap。

## 必须回写

- `C9M1-SCOPE-101`
- `C9M1-SCOPE-102`
- `C9M1-SCOPE-103`
- `C9M1-BG-101`
- `C9M1-BG-102`
- `C9M1-BLOCKER-301`
- README 的 S3 结论。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'handleOneSideOfJoint|offsetPlc|getGlobalPlacement|markerResolutionStatus|markerResolutionDiagnostic|unsupported_subshape_marker_primitive|Resolved subshape marker' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests/test_p8_features.py cad-core/fixtures/c3m6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
git diff --check
```

S3 关闭时，non-identity `offsetPlc` 和 non-AssemblyLink primitive frame 必须明确处于 `already_covered`、`oracle_candidate`、`backend_gap_candidate` 或 `diagnostic_non_goal` 之一。

## 非目标

- 不靠 fixture 名称、bbox、角度或输出顺序猜测 marker ownership。
- 不引入 persistent Assembly part cache。
- 不实现 GUI marker editing。
