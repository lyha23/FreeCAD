# C9-M1 S1 FreeCAD 源码与 current 覆盖候选

## 目标

复核 FreeCAD Assembly Joint marker / `offsetPlc` / placement writeback 源码和 current cad-core coverage，形成 C9-M1 source authority。

## 输入

- FreeCAD：`src/Mod/Assembly/App/AssemblyObject.cpp`
- FreeCAD：`src/Mod/Assembly/App/AssemblyObject.h`
- FreeCAD：`src/Mod/Assembly/App/AssemblyUtils.cpp`
- FreeCAD：`src/Mod/Assembly/App/AssemblyUtils.h`
- FreeCAD：`src/Mod/Assembly/JointObject.py`
- cad-core：`cad-core/src/assembly/joint_solver.cpp`
- cad-core：`cad-core/src/assembly/assembly_utils.cpp`
- cad-core：`cad-core/src/assembly/assembly_object.cpp`
- cad-core：`cad-core/src/runtime/capability_contract.cpp`
- tests / fixtures：`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6`

## 必须复核

- `handleOneSideOfJoint()` 的 reference resolution、object-global placement、part-local inverse 和 `offsetPlc` 顺序。
- `validateNewPlacements()` / `setNewPlacements()` 是否组合 `offsetPlc` 后写回 placement。
- `makeMbdJointOfType()`、`getJointType()`、`getJointCurrentValue()` 与 `JointObject.py` schema 是否覆盖 current supported JointType。
- current cad-core marker resolver 哪些路径已经 covered，哪些仍是 diagnostic / non-goal。
- current C3M6 expected / P8 tests 是否已经覆盖 non-identity marker chain、real Ondsel solver、documentObjectUpdates placement writeback。

## 必须回写

- `c9m1_assembly_marker_offset_source_candidates.tsv`
- `C9M1-BLOCKER-101`
- README 的 S1 结论段。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'handleOneSideOfJoint|offsetPlc|runPreDrag|setNewPlacements|validateNewPlacements|makeMbdJointOfType|getJointType|getJointCurrentValue|ObjectToGround|JointType' src/Mod/Assembly/App src/Mod/Assembly/JointObject.py cad-core/src/assembly cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M1-AssemblyJointMarkerOffsetPlacementRequestLocal主线/矩阵/*.tsv
git diff --check
```

S1 关闭时，`C9M1-SRC-201..403` 必须都有 source evidence、cad-core landing 和下一步 route；不得把候选直接写成 supported。

## 非目标

- 不新增 fixture。
- 不采 native oracle。
- 不改 capability。
- 不用 current cad-core 输出倒推 FreeCAD expected。
