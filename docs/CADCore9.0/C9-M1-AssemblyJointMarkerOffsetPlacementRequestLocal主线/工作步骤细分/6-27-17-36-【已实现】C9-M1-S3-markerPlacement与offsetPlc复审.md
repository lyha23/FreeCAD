# 【已实现】C9-M1 S3 markerPlacement 与 offsetPlc 复审

## 目标

复审 `handleOneSideOfJoint()` marker placement 与 `offsetPlc` 边界，决定 non-identity bundled `offsetPlc`、non-AssemblyLink subshape primitive frame 是否需要 oracle、实现或保持 non-goal。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`
- `src/Mod/Assembly/App/AssemblyObject.h::AssemblyObject::MbDPartData::offsetPlc`
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
- `C9M1-BG-103`
- `C9M1-BLOCKER-301`
- 必要时 `C9M1-NG-007`
- README 的 S3 结论。

## 复审结果

- `handleOneSideOfJoint()` 的 marker placement 顺序已复核：`getGlobalPlacement(nullptr, ref) * PlacementN` 先形成 object-global JCS，`getGlobalPlacement(part, ref).inverse()` 再转回 moving-part-local，最后 `data.offsetPlc * plc` 进入 `makeMbdMarker()`。这不是在 linked object local frame 里做猜测。
- `offsetPlc` 是 bundled parts 内部 offset：`AssemblyObject.h::MbDPartData::offsetPlc` 明确为 bundled parts 内部 offset，`getMbDData()` 在 fixed-joint bundling 中记录 `plc.inverse() * plci`，`validateNewPlacements()` / `setNewPlacements()` 使用 `getMbdPlacement(mbdPart) * offsetPlc`。C9-M1 不引入跨请求缓存或 frontend 补猜测。
- current `joint_solver.cpp` 覆盖 object-level baseline、AssemblyLink identity-offset subshape marker、Vertex / linear Edge / planar Face 和 mixed marker；非线性 Edge、非平面 Face 或缺 markerPlacement 的路径保持 `unsupported_subshape_marker_primitive` / `unsupported_assembly_solver` 诊断。
- C3M6 `assembly-marker-custom-placement-chain-real-solver.freecad.json` 已包含 non-identity connector / part placement chain native evidence，但 `offset_boundary` 是 `identity_offset_for_two_box_assembly_link_fixture`，当前 `test_p8_features.py` 没有直接断言该 fixture。它不能证明 non-identity bundled `offsetPlc`。
- S3 裁决：`C9M1-SCOPE-101` / `C9M1-BG-101` 保持 `already_covered`；`C9M1-SCOPE-102` / `C9M1-BG-102` 保持 `oracle_candidate`，collector / probe 基线应是 fixed-joint bundle 产生 non-identity `objectPartMap.offsetPlc` 的 native case，本步按非目标不采集；`C9M1-SCOPE-103` / `C9M1-BG-103` 保持 `diagnostic_non_goal`。`C9M1-BLOCKER-301` 已关闭为 `Closed S3`。

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
