# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S1 FreeCAD 源码候选矩阵

## 目标

用 live FreeCAD 源码和当前 `cad-core` 落点复核 source candidates，证明本包覆盖同一条 marker placement 调用链：ordinary dispatch、Reference / Placement validation、object/subshape global transform、moving part local transform、`offsetPlc`、mixed DistanceType ordering、RackPinion special rewrite、real Ondsel marker consumption、native oracle collector 和 capability publication surface。

S1 只建立 source authority 和后续 route，不把任何 scope 写成 `supported`。

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
62280cb35d

git log -1 --oneline
62280cb35d docs: 完成 P8 MarkerPlacement S0 基线复核

git -c core.quotepath=false status --short -uall
<clean>
```

## 源码复核结论

| 轴 | live source authority | S1 结论 |
| --- | --- | --- |
| ordinary dispatch / RackPinion 特例 | `src/Mod/Assembly/App/AssemblyObject.cpp:1408-1428` | `makeMbdJoint()` 先 `makeMbdJointOfType()`，普通 Joint 分别调用 `handleOneSideOfJoint(joint, "Reference1", "Placement1")` / `Reference2`；`JointType::RackPinion` 先走 `getRackPinionMarkers()`。 |
| ReferenceN validation | `src/Mod/Assembly/App/AssemblyObject.cpp:1658-1675` | `handleOneSideOfJoint()` 同时要求 moving part、linked object 和 `PropertyXLinkSub` 可用；失败返回空 marker name 并 warning，不应在 `cad-core` 中静默补 connector-only marker。 |
| `PlacementN` 到 reference global | `src/Mod/Assembly/App/AssemblyObject.cpp:1669-1681` | `PlacementN` 是 object-relative JCS；FreeCAD 用 `getGlobalPlacement(nullptr, ref)` 把它转到 reference object / subshape global。 |
| reference global 到 moving part local | `src/Mod/Assembly/App/AssemblyObject.cpp:1682-1684` | FreeCAD 再用 `getGlobalPlacement(part, ref).inverse()` 转回 containing moving part local marker frame。 |
| `offsetPlc` | `src/Mod/Assembly/App/AssemblyObject.cpp:1686-1689` | bundled part offset 在 part-local marker 后应用；实现前必须保留 backend gap 或 diagnostic，不能把 connector placement 当等价结果。 |
| RackPinion rewrite | `src/Mod/Assembly/App/AssemblyObject.cpp:1698-1788` | RackPinion 先通过 `slidingPartIndex()` 必要时 `swapJCS()`，pinion 侧仍走 `handleOneSideOfJoint()`；rack 侧用 pinion / rack global placement 计算相对旋转，调整 rack marker 使 Z 轴跟 pinion、X 轴跟 sliding axis，并同样处理 part local 与 `offsetPlc`。 |
| mixed ordering | `src/Mod/Assembly/App/AssemblyUtils.cpp:61-84`、`154-371` | `swapJCS()` 会同时交换 `Placement1/2` 与 `Reference1/2`；`getDistanceType()` 在 Point / Line / Plane mixed 组合中会触发 swap。`cad-core` 只能 request-local swap DTO，必须同步 connector / marker placement。 |
| current value | `src/Mod/Assembly/App/AssemblyUtils.cpp:811-834` | `getJointCurrentValue()` 先把 `Placement1/2` 分别乘 `GeoFeature::getGlobalPlacement(nullptr, ref1/ref2)`，再按两个 JCS global placement 的相对 placement 计算 Distance / Angle 初值。 |
| `cad-core` marker gap | `cad-core/src/assembly/joint_solver.cpp:640-652` | `jointReference()` 当前只读 `ReferenceN` / `PlacementN`，并执行 `reference.markerPlacement = reference.connectorPlacement`；这是 S2/S3/S5 的 backend gap，不是支持结论。 |
| real Ondsel consumption | `cad-core/src/assembly/joint_solver.cpp:981-1002` | `addConstraintToOndselAssembly()` 用 `joint.reference1/2.markerPlacement` 创建 marker I/J；resolver 输出会影响真实 solver path，不只是 debug evidence。 |
| request build order | `cad-core/src/assembly/joint_solver.cpp:1166-1242` | `buildAssemblySolveRequest()` 先 `jointReference()`，再 `classifyDistanceType()` / Screw / RackPinion scalar、`resolveDistanceJointMapping()`，最后执行 sliding-side precondition 和 RackPinion rewrite；后续实现必须在这些 request-local rewrite 后保持 marker 同步。 |
| collector route | `cad-core/tools/collect_freecad_expected.py:1522-1591`、`1594-1660` | collector 已有 fixture-side DistanceType / swap evidence 和 native solver `placement_updates` payload，S4 需要扩展 representative marker evidence / expected，而不是 S1 采集。 |
| capability route | `cad-core/src/adapters/c_api/c_api.cpp:150-205` | 当前 capability 只发布 Ondsel solver 子集、`basic_distance_type` 和 DistanceType scalar / class support；没有 subshape marker placement covered key，必须留给 S6 发布闸门。 |

## candidate / route 回写

- `p8_marker_placement_source_candidates.tsv` 仍保持 20 条候选，覆盖 dispatch / validation / object-global / part-local / offset / current-value / mixed-swap / special-rewrite / real-Ondsel-consumption / oracle / capability。
- `p8_marker_placement_scope_review_matrix.tsv` 仍保持 14 个 scope；`supportedBaseline` 只限 object-level native placement baseline，`backendGap` / `notCollected` / `releaseGate` / `nonGoal` 状态不因 S1 改成 supported。
- 两份矩阵的 route / next_step 已从 S1 source review 进入 S2-S6：S2 做范围准入，S3 设计 resolver，S4 采 native oracle，S5 落实现与 focused parity，S6 发布 capability/docs。

## S1 状态裁决

- S1 已完成：source candidates 已由 live FreeCAD / cad-core 源码复核。
- S2-S6 尚未执行：不得提前标记 resolver、oracle、C++ implementation、focused parity 或 capability publication 为 supported。
- 不修改 C++，不采集 oracle，不新增 fixture，不改 expected。

## 验收

```bash
rg -n "handleOneSideOfJoint|getRackPinionMarkers|getJointCurrentValue|getDistanceType|swapJCS|getGlobalPlacement|markerPlacement = reference.connectorPlacement|addConstraintToOndselAssembly|buildAssemblySolveRequest|basic_distance_type" src/Mod/Assembly/App cad-core/src/assembly cad-core/include/cad_core/assembly cad-core/src/adapters/c_api/c_api.cpp cad-core/tools/collect_freecad_expected.py
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

## 非目标

- S1 不改 C++。
- S1 不采 oracle。
- S1 不把候选写成 supported。
