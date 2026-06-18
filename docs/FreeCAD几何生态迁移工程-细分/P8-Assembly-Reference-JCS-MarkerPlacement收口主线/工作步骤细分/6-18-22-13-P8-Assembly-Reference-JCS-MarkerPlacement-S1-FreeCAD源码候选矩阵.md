# P8 Assembly Reference / JCS MarkerPlacement S1 FreeCAD 源码候选矩阵

## 目标

用 FreeCAD 源码和当前 cad-core 落点建立候选矩阵，证明本包是同一条 marker placement 调用链的完整语义批次，而不是单个 Distance fixture 修复。

## 必须复核的 FreeCAD 依据

| 候选 | 源码 | 验证点 |
| --- | --- | --- |
| `makeMbdJoint()` | `src/Mod/Assembly/App/AssemblyObject.cpp` | 普通 Joint 调 `handleOneSideOfJoint()`；RackPinion 特例 |
| `handleOneSideOfJoint()` | `src/Mod/Assembly/App/AssemblyObject.cpp` | `PlacementN`、`getGlobalPlacement(nullptr, ref)`、`getGlobalPlacement(part, ref).inverse()`、`offsetPlc` |
| `getRackPinionMarkers()` | `src/Mod/Assembly/App/AssemblyObject.cpp` | rack/pinion marker rewrite 前置和 yaw adjustment |
| `getJointCurrentValue()` | `src/Mod/Assembly/App/AssemblyUtils.cpp` | Distance / Angle initial value 依赖 reference global placement |
| `getDistanceType()` / `swapJCS()` | `src/Mod/Assembly/App/AssemblyUtils.cpp` | mixed reference 排序必须同步 reference 和 placement |
| cad-core `jointReference()` | `cad-core/src/assembly/joint_solver.cpp` | 当前缺口为 `markerPlacement = connectorPlacement` |
| cad-core `addConstraintToOndselAssembly()` | `cad-core/src/assembly/joint_solver.cpp` | solver path 实际消费 `reference1/2.markerPlacement` 创建 marker |
| cad-core `buildAssemblySolveRequest()` | `cad-core/src/assembly/joint_solver.cpp` | `jointReference()` 早于 DistanceType classification，后续 swap 必须同步 placement |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 能否批量采集 representative `placement_updates` |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 当前尚无 subshape marker placement covered key |

## 必须回写

- `p8_marker_placement_source_candidates.tsv`
- `p8_marker_placement_scope_review_matrix.tsv` 的 source candidate 路由字段
- 若候选少于 dispatch / validation / object-global / part-local / offset / current-value / mixed-swap / special-rewrite / real-Ondsel-consumption / oracle / capability 这些轴，S1 不能收口。

## 验收

```bash
rg -n "handleOneSideOfJoint|getRackPinionMarkers|getJointCurrentValue|getDistanceType|swapJCS|getGlobalPlacement" src/Mod/Assembly/App cad-core/src/assembly cad-core/include/cad_core/assembly
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
```

## 非目标

- S1 不改 C++。
- S1 不采 oracle。
- S1 不把候选写成 supported。
