# 【已实现】C9-M4 S1 FreeCAD 源码与 current 覆盖候选

## 目标

复核缺 oracle default rows 的 FreeCAD source authority、current cad-core 落点、collector 行为、fixture inventory 和 diagnostics guard。S1 只形成 source candidates，不把候选直接写成 supported 或 backendGap。

## FreeCAD 依据

| 语义 | 源码 | S1 要确认 |
| --- | --- | --- |
| DistanceType 分类 | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | Face / Face、Vertex / Face、Edge / Face 如何进入 13 个 default rows。 |
| default solver | `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` | default 是否创建 `ASMTPlanarJoint` 并写 `offset`。 |
| current DTO | `cad-core/src/assembly/joint_solver.cpp` | `classifyDistanceType()`、`resolveDistanceJointMapping()` 当前如何处理 missing default rows。 |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | default boundary metadata 是否仍保护未采 rows。 |
| capability / tests | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests` | supported rows 与 missing rows 是否可区分。 |

## 必须回写的矩阵行

- `C9M4-SRC-101..402`
- `C9M4-SCOPE-101..701` 的 source linkage
- `C9M4-BLOCKER-101`

## 关闭证据

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=825ad7f937`，`git log -1 --oneline` 为 `825ad7f937 docs: 关闭 C9-M4 S0 基线冻结`，S1 起始 `git -c core.quotepath=false status --short -uall` 无输出。队列复核显示 S1-S6 pending。
- FreeCAD `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` 覆盖 13 个 C9-M4 missing default rows：
  - Face / Face：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere` 来自 Face/Face cylinder/cone ordering；源码在需要时调用 `swapJCS(joint)` 保证 solver-side ordering。
  - Vertex / Face：`PointCone`、`PointTorus` 来自 Vertex/Face 分支，Vertex-first 时先 `swapJCS(joint)` 让 Face 成为第一侧。
  - Edge / Face line：`LineSphere`、`LineCone`、`LineTorus` 来自 Edge/Face 分支，Edge-first 时先 `swapJCS(joint)`，line edge 走 `Line*`。
  - Edge / Face non-line：`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 来自同一 Edge/Face 分支的 non-line fallback，FreeCAD 注释为 “For other curves we consider them as planes for now. Can be refined later.”
- FreeCAD `src/Mod/Assembly/App/AssemblyObject.cpp::makeMbdJointDistance()` 的 default branch 明确创建 `ASMTPlanarJoint` 并写 `offset = getJointDistance(joint)`；S1 只把它记录为 source authority，不把缺 oracle rows 直接写 supported/backendGap。
- current `cad-core/src/assembly/joint_solver.cpp` 已能按 FreeCAD ordering 分类这些 rows，并通过 `recordDistanceTypeEvidence()` 把未显式支持的 default rows 标成 `distanceTypeMappingStatus=default_boundary_not_mapped`、`distanceTypeBoundary=default_or_todo_boundary`。`resolveDistanceJointMapping()` 只把 C9-M3 accepted default rows `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 映射为 `ASMTPlanarJoint + offset`；13 个 C9-M4 rows 不在 accepted set。
- collector `cad-core/tools/collect_freecad_expected.py` 识别完整 default boundary family，但 `distance_type_is_accepted_default_planar()` 只接受 C9-M3 rows；其他 default rows 继续由 `mark_default_distance_boundary()` 写 `default_boundary_not_mapped/default_or_todo_boundary`，并在 diagnostic review metadata 中阻止它们进入 supported capability。
- capability/tests 当前能区分 supported 与 missing：`cad-core/src/runtime/capability_contract.cpp` 发布 `native_expected_count=18`，supported 包含 C9-M3 accepted rows，`default_or_todo_boundaries` 仍列出 13 个 C9-M4 rows；`cad-core/tests/test_adapters.py` 断言 accepted default rows 不在 default list 且 13 个 missing rows 均在 default list；`cad-core/tests/test_p8_features.py` 已有 `CurveCylinder` current guard，证明未采 default row 保持无 solver class 且 reason 为 `default_boundary_not_mapped`。
- `cad-core/fixtures/c3m6` inventory 对 13 个 C9-M4 row 名称无 input / expected 命中；只存在 C9-M3 accepted default rows `PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 和 `PointCurve` 的 checked-in expected。
- 已回写 `矩阵/c9m4_distance_type_default_missing_oracle_source_candidates.tsv` 的 `C9M4-SRC-101..402`、`矩阵/c9m4_distance_type_default_missing_oracle_scope_review_matrix.tsv` 的 `C9M4-SCOPE-101..701` source linkage/current evidence，以及 `矩阵/c9m4_distance_type_default_missing_oracle_blocker_queue.tsv` 的 `C9M4-BLOCKER-101`。S1 未采 native oracle，未新增或修改 cad-core C++、fixtures、expected、tests，未把候选写成 supported/backendGap。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'getDistanceType|makeMbdJointDistance|CylinderCone|ConeCone|ConeTorus|ConeSphere|PointCone|PointTorus|LineSphere|LineCone|LineTorus|CurveCylinder|CurveSphere|CurveCone|CurveTorus|ASMTPlanarJoint|default_or_todo_boundary' src/Mod/Assembly/App cad-core/src/assembly cad-core/tools/collect_freecad_expected.py cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6 docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/矩阵/*.tsv
git diff --check
```

## 验收标准

- source candidates 覆盖 FreeCAD 分类、FreeCAD default solver mapping、cad-core DTO / guard、collector / fixture、capability tests。
- 每条 source candidate 都有 source evidence、cad-core landing 和 owner step。
- S1 不采集 oracle、不改 expected、不改 C++，也不把 candidate 标成 backendGap。

## 非目标

- 不设计新的 DistanceType 枚举。
- 不引入非 FreeCAD 的几何猜测分类。
- 不处理 non-AssemblyLink primitive frame DTO。
