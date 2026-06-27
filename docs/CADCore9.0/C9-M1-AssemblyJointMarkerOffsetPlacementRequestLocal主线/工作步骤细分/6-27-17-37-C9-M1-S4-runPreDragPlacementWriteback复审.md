# C9-M1 S4 runPreDrag Placement Writeback 复审

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
