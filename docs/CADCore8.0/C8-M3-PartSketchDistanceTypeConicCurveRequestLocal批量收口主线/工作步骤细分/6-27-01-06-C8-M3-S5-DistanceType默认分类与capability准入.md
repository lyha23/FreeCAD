# C8-M3-S5 DistanceType 默认分类与 capability 准入

## 目标

查清 `part_workbench.conic_curves.remaining_gaps` 中的 `distance_type_default_todo`：它到底是实现缺口、capability publication stale gap、oracle blocker，还是 non-goal。S5 可打开 S6 implementation gate，但不直接实现。

## FreeCAD 依据

| 入口 | 复核重点 |
| --- | --- |
| `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | reference geometry -> DistanceType |
| `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointOfType()` | DistanceType -> solver joint |
| `src/Mod/Part/App/Attacher.h` | conic reference types |
| `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | conic external reference shape typing |

## current cad-core 复核

- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/assembly/assembly_utils.cpp`
- `cad-core/tests/test_p8_features.py` DistanceType cases
- `cad-core/tests/test_adapters.py` capability assertions
- `cad-core/src/runtime/capability_contract.cpp`

## 必须回写的矩阵行

- `C8M3-SCOPE-301`
- `C8M3-BG-101`
- `C8M3-BLOCKER-501`
- `C8M3-VAL-501`

## 验收标准

- `distance_type_default_todo` 有明确裁决：`backend_gap_candidate`、`capability_publication_gap`、`oracle_blocked` 或 `non_goal`。
- 如果是 `backend_gap_candidate`，必须列出 C++ landing、fixture/test route 和 success criteria。
- 如果是 `capability_publication_gap`，必须证明 current tests 已覆盖，不需要新增 C++。
- 如果保留 blocker，必须写 delete/reopen condition。
- 运行：

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'DistanceType|getDistanceType|PointCurve|Other|distance_type_default_todo|distance_type_mapping_status|C8M3-BG-101|C8M3-SCOPE-301' src/Mod/Assembly/App cad-core/src/assembly cad-core/tests docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

验收通过后，将本文件重命名为 `6-27-01-06-【已实现】C8-M3-S5-DistanceType默认分类与capability准入.md`。

## 非目标

- 不实现 full Assembly solver。
- 不把 conic curve default 分类靠字符串猜测。
- 不修改下游 Rust。
