# 【已实现】C9-M1 S4 runPreDrag Placement Writeback 复审

## 目标

复审 real Ondsel `runPreDrag()`、`setNewPlacements()`、`validateNewPlacements()` 与 cad-core `documentObjectUpdates.action=assembly_set_placement` 的 request-local writeback 边界。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::solve()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::validateNewPlacements()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::setNewPlacements()`
- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJointOfType()`
- `src/Mod/Assembly/App/AssemblyUtils.cpp::getJointCurrentValue()`

## 必须复核

- cad-core 是否只发布 request-local placement update 建议，而不是后端持久写回。
- placement update 是否已经由 C3M6 expected 覆盖，并且是否包含 `offsetPlc` 组合。
- zero Angle fallback class evidence 是否只是 oracle gap，还是有 current C++ mismatch。
- unsupported JointType diagnostic 是否仍准确，不应因 C9-M1 扩面误删。
- S4 是否需要打开 S6 code gate，或只给 S5 capability 发布证据。

## 必须回写

- `C9M1-SCOPE-201`
- `C9M1-SCOPE-202`
- `C9M1-SCOPE-203`
- `C9M1-BG-201`
- `C9M1-BG-202`
- `C9M1-BLOCKER-401`
- README 的 S4 结论。

## 复审结果

- S4 执行基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=292587a0f5`（`292587a0f5 docs: 完成 C9-M1 S3 markerPlacement 复审`），开始状态干净。
- FreeCAD `solve()` 顺序仍是 `fixGroundedParts()` / `jointParts()` 后调用 `mbdAssembly->runPreDrag()`，随后 `setNewPlacements()`；拖拽路径才先跑 `validateNewPlacements()`。`validateNewPlacements()` 与 `setNewPlacements()` 都先读 `getMbdPlacement(mbdPart)`，再在 `offsetPlc` 非 identity 时组合 `getMbdPlacement(mbdPart) * offsetPlc`。
- cad-core writeback 边界已复核为 request-local：`solveAssemblyWithRealOndselAdapter()` 调用 real Ondsel `runPreDrag()` 后只构造 `AssemblyPlacementUpdate`；`assembly_utils.cpp::placementUpdateJson()` 发布 `documentObjectUpdates.action=assembly_set_placement`；`solverSummary()` 把更新推入 `context.documentObjectUpdates`；`assembly_object.cpp` 只把同一 request 的 placement update 应用于 Assembly display compound summary，不持久保存后端 solver state、placement cache 或 frontend graph。
- C3M6 expected 覆盖 placement update native evidence：`cad-core/fixtures/c3m6/expected` 中 42 个文件、44 条 `solver_adapter.placement_updates.action=assembly_set_placement`。`test_p8_features.py` 进一步断言顶层 `documentObjectUpdates`、next-request apply 后 no-op、多组件 writeback 顺序、partial writeback 和 unsupported diagnostic；`test_adapters.py` 断言 capability 发布 `request_local_runPreDrag` 与 `documentObjectUpdates.action=assembly_set_placement`。
- `offsetPlc` 顺序有 FreeCAD source 证据，但 non-identity bundled `offsetPlc` native oracle 仍沿用 S3 route：现有 custom-placement-chain expected 的 `offset_boundary` 是 `identity_offset_for_two_box_assembly_link_fixture`，不能证明 non-identity bundled `offsetPlc`，也不构成 backendGap。
- zero Angle fallback 只有 source/class evidence：FreeCAD `makeMbdJointOfType()` 对 zero / 2π Angle 走 `ASMTParallelAxesJoint`，cad-core 当前对 exact zero Angle 也返回 `ASMTParallelAxesJoint`。`assembly-angle-zero-and-signed-current-real-solver.json` 只是输入 fixture，没有对应 expected 或测试引用；C9-M1 未采 native zero Angle oracle，也没有 current C++ mismatch 证据。`C9M1-SCOPE-203` / `C9M1-BG-201` 保持 `known_gap_retained`，不打开 S6 C++ gate。
- unsupported diagnostics 仍准确：unsupported JointType、PointCurve boundary、缺 marker placement 或缺 solver joint class 仍走 `unsupported_assembly_solver`，测试覆盖 `point_curve_diagnostic_boundary` 与 invalid RackPinion sliding precondition，不会静默降级为 success。

## S5 / S6 route

- S5：消费 S3/S4 route，最多做 capability / diagnostics 发布口径复核；不得重写 adapter 字符串来隐藏 `non_identity_bundled_offsetPlc` 或 primitive-frame non-goal。
- S6：当前 S4 不打开 runtime C++ 实现 gate；若 S5 不发现发布口径缺口，S6 应走 no-code release gate，只跑队列、TSV、whitespace、`git diff --check` 和必要 focused tests。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'runPreDrag|setNewPlacements|validateNewPlacements|assembly_set_placement|documentObjectUpdates|unsupported_assembly_solver|ondsel_solver_failed|zero Angle|Angle' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
git diff --check
```

S4 关闭时，placement writeback 必须明确是否 `already_covered`；zero Angle fallback 必须保持 `oracle_candidate` / `known_gap_retained`，除非有 native oracle 和 current mismatch。

## 非目标

- 不引入 persistent solver state。
- 不修改 frontend graph；只允许 `documentObjectUpdates` 建议。
- 不把 unsupported JointType 静默降级为 success。
