# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S0 声明口径与 live 基线复核

## 目标

冻结本包的支持声明、禁止声明、状态字典和 live baseline，避免把本包误写成 radius-bearing DistanceType、完整 Assembly transaction 或 GUI/session 支持。

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
e7d6b024f9

git log -1 --oneline
e7d6b024f9 docs: 完成P8 DistanceType S6发布边界审计

git -c core.quotepath=false status --short -uall
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement收口主线总入口.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement工作步骤总入口.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-12-P8-Assembly-Reference-JCS-MarkerPlacement-S0-声明口径与live基线复核.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-13-P8-Assembly-Reference-JCS-MarkerPlacement-S1-FreeCAD源码候选矩阵.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-14-P8-Assembly-Reference-JCS-MarkerPlacement-S2-范围准入与blocker矩阵.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-15-P8-Assembly-Reference-JCS-MarkerPlacement-S3-MarkerPlacementResolver专项复审.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-16-P8-Assembly-Reference-JCS-MarkerPlacement-S4-NativeOracle与代表fixture专项复审.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-17-P8-Assembly-Reference-JCS-MarkerPlacement-S5-实现与focused-parity.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-18-P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_backend_gap_classification.tsv
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_blocker_queue.tsv
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_non_goal_registry.tsv
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_scope_review_matrix.tsv
?? docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_source_candidates.tsv
```

本轮开始时，本 P8 package 全部为未跟踪文件；S0 只允许更新本包文档状态，不 reset、revert 或清理这些输入文件。

## 输入复核

- `git status --short`
- `cad-core/include/cad_core/assembly/joint_solver.h`
- `cad-core/src/assembly/joint_solver.cpp`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/fixtures/c3m6/expected`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`
- `docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线`
- `docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv`

## S0 复核结论

| 问题 | 结论 | live 证据 |
| --- | --- | --- |
| `jointReference()` 是否仍是 connector-only marker baseline | 是。当前 `cad-core/src/assembly/joint_solver.cpp::jointReference()` 仍只读取 `ReferenceN` 和 `PlacementN`，并执行 `reference.markerPlacement = reference.connectorPlacement`。 | `rg` 命中 `cad-core/src/assembly/joint_solver.cpp:651`；FreeCAD 普通路径仍是 `AssemblyObject.cpp::makeMbdJoint()` 调 `handleOneSideOfJoint()`，后者执行 object/subshape global -> part local -> `offsetPlc`。 |
| object-level native placement expected 是否仍为 supportedBaseline | 是。它是本包保护的已支持 baseline，不是 subshape marker placement 结论。 | 本包 `MP-SCOPE-001` 为 `supportedBaseline`；前置 AssemblySolver S6/S7 文档把 native solver placement oracle 和 `P8ASM-SCOPE-006` 收口为 supported；当前 c3m6 expected 无 `known_gap` 命中。 |
| basic DistanceType 是否只发布 DTO / class / scalar | 是。DistanceTypeBasicGeometry S6 只发布 `distance_type`、resolved Ondsel class、`distance_ij` / `offset`、request-local `jcs_swapped_for_solver` 和 C ABI `basic_distance_type`，不声明 full native subshape marker placement parity。 | `cad-core/src/adapters/c_api/c_api.cpp::ondselSolverCapabilityJson()` 只发布 `basic_distance_type` / `distance_type_basic_geometry`，没有 subshape marker placement capability。 |
| RackPinion / Screw special rewrite 是否已支持但仍需保护 | 是。RackPinion / Screw 已在独立子线作为 request-local scalar supported 子集发布；本包后续 resolver 切换必须把它们作为 regression gate。 | `cad-core/src/assembly/joint_solver.cpp::isSupportedOndselJointType()` 包含 `RackPinion` / `Screw`；`applyRackPinionMarkerRewrite()` 已保留 FreeCAD marker rewrite 注释与 DTO 证据；本包 `MP-SCOPE-010` 仍为 `releaseGate`。 |
| connector-only shortcut 是否只能是 gap / nonGoal | 是。object-level baseline 可继续作为回归保护，但不能把 `PlacementN` 直传 `markerPlacement` 写成 subshape support。 | `p8_marker_placement_non_goal_registry.tsv` 的 `MP-NG-006` 明确 connector-only marker shortcut 不能作为 supported placement parity 发布。 |
| 矩阵规模是否保持最小完整语义批次 | 是。当前矩阵仍为 20 candidates、14 scope、12 backend/release 分类、10 blockers；non-goal registry 为 6 条排除边界。 | `wc -l` 去 header 计数：source candidates 20、scope review 14、backend gap classification 12、blocker queue 10、non-goal 6；TSV 字段数 `awk` 检查无输出。 |

## S0 状态裁决

- S0 已完成：本包当前只完成声明口径与 live baseline 复核。
- S1-S6 尚未执行：不得提前标记 source matrix、scope review、resolver、oracle、C++ implementation 或 capability 发布为 supported。
- 不修改 C++，不采集 oracle，不新增 fixture，不改 capability。
- `MP-SCOPE-005/006/007/009/012` 继续保持 `notCollected`；`MP-SCOPE-002/003/004/008` 继续保持 `backendGap`；`MP-SCOPE-010/011/013` 继续保持 `releaseGate`；`MP-SCOPE-014` 继续保持 `nonGoal`。

## 声明口径

以下是本包完成 S4-S6 oracle / implementation / capability 闸门后才允许发布的范围；S0 当前不把这些写成 `supported`。

| 项 | 本包完成后允许声明 | 禁止声明 |
| --- | --- | --- |
| subshape marker placement | 已发布 Assembly solver 子集中，Vertex / Edge / Face / mixed reference 的 marker placement 可按 FreeCAD `handleOneSideOfJoint()` 语义计算 | 不声明所有 FreeCAD Assembly GUI / session 行为 |
| DistanceType | basic Point / Line / Plane DistanceType 可在本批补 full placement parity | 不声明 radius-bearing 或 curve/default DistanceType |
| special rewrite | RackPinion / Screw special marker rewrite 不回退 | 不把 special rewrite 改成输出端 fixture patch |
| state boundary | 所有 marker evidence 都是 request-local | 不持久保存 solver session、Shape、NamedShape、ElementMap、BREP |
| connector-only shortcut | object-level baseline 可作为回归保护 | 不把 subshape ref 的 `PlacementN` 直传 `markerPlacement` 当成 supported |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `supportedBaseline` | 已有已验证能力，本包只保护不回退 |
| `backendGap` | 有 FreeCAD source authority 和当前 cad-core mismatch evidence，需要实现 |
| `notCollected` | 属于本包但缺 FreeCADCmd expected 或 focused fixture |
| `releaseGate` | 实现完成后必须同步 tests / expected / capability / docs 才能发布 |
| `nonGoal` | 本包明确排除，必须写 reopen condition |

## 验收

```bash
git status --short
rg -n "markerPlacement = reference.connectorPlacement|handleOneSideOfJoint|getRackPinionMarkers|getJointCurrentValue|basic_distance_type" cad-core/src/assembly/joint_solver.cpp src/Mod/Assembly/App/AssemblyObject.cpp src/Mod/Assembly/App/AssemblyUtils.cpp cad-core/src/adapters/c_api/c_api.cpp
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
```

## 非目标

- S0 不修改 C++。
- S0 不采集 native oracle。
- S0 不把任何 scope 改成 supported。
