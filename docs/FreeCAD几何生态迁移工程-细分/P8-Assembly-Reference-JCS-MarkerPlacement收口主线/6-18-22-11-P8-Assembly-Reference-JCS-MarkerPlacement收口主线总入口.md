# P8 Assembly Reference / JCS MarkerPlacement 收口主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P8 Assembly 后续收口实施包。它处理已发布 request-local Assembly solver 子集中，Joint `Reference1` / `Reference2` 到 subshape JCS，再到 Ondsel marker placement 和 `placement_updates` 的 native parity。

对应上游方案入口是 `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeBasicGeometry-OndselSolver收口主线`、`docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线` 和 `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`。

## 主线目标

- 迁移 FreeCAD `AssemblyObject.cpp::handleOneSideOfJoint()` 的 request-local marker placement 语义：`PlacementN` 先转到 reference object / subshape global，再转回 moving part local，并保留 `MbDPartData.offsetPlc` 边界。
- 批量覆盖 Vertex / Edge / Face / mixed reference 的 representative fixtures，而不是只修某个 DistanceType fixture。
- 将 `getJointCurrentValue()` 的 JCS global placement 证据纳入 mixed / Distance / Angle 验收，避免只验证 solver DTO、不验证 placement-derived scalar 的来源。
- 保持 RackPinion / Screw 已有 special marker rewrite 和 sliding-side swap 行为不回退。
- 将 basic DistanceType 从“solver DTO / class / scalar parity”推进到 representative subshape `placement_updates` parity。
- 发布 C ABI capability / docs 时只声明已验证的 subshape marker placement 子集，不发布 radius-bearing DistanceType、curve/default、GUI/session 或 persistent solver state。

## 当前基线

- P8 AssemblySolver 主线已完成 real Ondsel solver、object-level native placement parity、writeback lifecycle 和 C ABI publication。
- P8 Cylindrical、Parallel / Perpendicular、Gears / Belt、Screw / RackPinion 和 basic DistanceType 子线均已收口为 request-local supported 子集。
- `cad-core/src/assembly/joint_solver.cpp::jointReference()` 当前仍把 `markerPlacement` 直接设为 `PlacementN`，这对 object-level baseline 可用，但没有完整表达 FreeCAD `handleOneSideOfJoint()` 的 subshape reference 坐标系换算；该 connector-only shortcut 已在 non-goal registry 中标为不得发布的已知缺口。
- DistanceType S6 已明确：S5 native oracle 只覆盖 solver DTO、resolved Ondsel class、`distance_ij` / `offset` 和 request-local `jcs_swapped_for_solver`，不声明 full native subshape marker placement parity。
- 本包现在按最小完整语义批次组织：20 条 source candidates、14 个 scope、12 类 backend/release 分类、10 个发布前 blocker。后续实现不得退回只采单个 oracle case 或只修单个 fixture。
- S0 已完成 live 复核：上述三条仍成立；object-level native placement 只是 `supportedBaseline`，RackPinion / Screw 已支持子集只作为本包后续 regression gate，S2-S6 仍不得提前写成 supported。
- S1 已完成 live 源码候选矩阵复核：20 条 candidates 和 14 个 scope 只代表 source authority / route，不代表 supported；S2-S6 仍不得提前写成 supported。

## 证明链条

```text
声明口径
  -> FreeCAD marker/JCS 源码候选
  -> scope review / nonGoal / blocker queue
  -> marker placement resolver 专项复审
  -> native oracle / representative fixtures 专项复审
  -> cad-core resolver implementation and focused parity
  -> capability/docs 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| Joint 到 marker 分发 | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::makeMbdJoint()` | 普通 Joint 调 `handleOneSideOfJoint(joint, "Reference1", "Placement1")` 和 `handleOneSideOfJoint(joint, "Reference2", "Placement2")`；RackPinion 走 `getRackPinionMarkers()` |
| 单侧 marker placement | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()` | 读取 moving part、linked object 和 `PlacementN`，用 `getGlobalPlacement(nullptr, ref)` 转到 reference global，再用 `getGlobalPlacement(part, ref).inverse()` 转回 moving part local |
| bundled part offset | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()` | part-local marker 再按 `data.offsetPlc` 调整；cad-core 若不能支持，必须显式 diagnostic，不能 silent fallback |
| RackPinion 特例 | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::getRackPinionMarkers()` | rack 作为 marker I，pinion 作为 marker J；rack marker Z 轴对齐 pinion，X 轴沿 sliding axis |
| current value | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getJointCurrentValue()` | `Placement1/2` 先乘 `GeoFeature::getGlobalPlacement(nullptr, ref)`，Distance/Angle 初值按两个 JCS 的相对 placement 计算 |
| DistanceType swap | `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()` | 点 / 线 / 面组合可 `swapJCS(joint)`，cad-core 只能做 request-local DTO ordering，不持久修改 graph |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| assembly DTO | `cad-core/include/cad_core/assembly/joint_solver.h` | 保留 `connectorPlacement`，补充 marker resolver evidence / diagnostic 字段，并表达 resolver 是否消费了 object/global、part-local 和 offset 边界 |
| request builder | `cad-core/src/assembly/joint_solver.cpp::jointReference()` | 用 FreeCAD 等价 resolver 替代 `markerPlacement = connectorPlacement`，并保证 request-local swap 后 reference / connector / marker 同步 |
| reference placement | `cad-core/src/assembly`，必要时 `cad-core/src/part` / `cad-core/src/topo` | 解析 `VertexN` / `EdgeN` / `FaceN` 的 request-local position、axis、normal 和 containing part transform |
| current value evidence | `cad-core/src/assembly/joint_solver.cpp`、fixtures expected | 让 Distance / Angle scalar 的来源与 subshape JCS placement 保持可追溯 |
| special rewrite | `cad-core/src/assembly/joint_solver.cpp` | 保持 RackPinion / Screw special marker rewrite，统一 resolver 只提供输入 marker placement |
| expected collector | `cad-core/tools/collect_freecad_expected.py` | 为本批 representative fixtures 采集 native `placement_updates` 和必要 marker evidence |
| capability | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 `subshape_marker_placement` 或等价 covered key，并保留 radius / curve / GUI / session 排除 |
| tests / fixtures | `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6` | 锁定 Vertex / Edge / Face / mixed / special rewrite representative parity |
| upstream docs | `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`P8-DistanceTypeBasicGeometry-OndselSolver收口主线` | 回写 supported subset 和 remaining boundary |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-18-22-12-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S0-声明口径与live基线复核.md` | 已完成：冻结 claims、非目标和 current marker baseline |
| S1 FreeCAD 源码候选 | `工作步骤细分/6-18-22-13-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S1-FreeCAD源码候选矩阵.md` | 已完成：建立 FreeCAD / cad-core source candidates |
| S2 范围准入 | `工作步骤细分/6-18-22-14-P8-Assembly-Reference-JCS-MarkerPlacement-S2-范围准入与blocker矩阵.md` | 将候选路由到 backendGap、notCollected、releaseGate、nonGoal |
| S3 marker resolver 复审 | `工作步骤细分/6-18-22-15-P8-Assembly-Reference-JCS-MarkerPlacement-S3-MarkerPlacementResolver专项复审.md` | 复审统一 resolver、diagnostic 和 special rewrite 边界 |
| S4 native oracle 复审 | `工作步骤细分/6-18-22-16-P8-Assembly-Reference-JCS-MarkerPlacement-S4-NativeOracle与代表fixture专项复审.md` | 批量采集 representative native expected |
| S5 实现与 parity | `工作步骤细分/6-18-22-17-P8-Assembly-Reference-JCS-MarkerPlacement-S5-实现与focused-parity.md` | 切换 cad-core 主路径并补 focused tests |
| S6 发布闸门 | `工作步骤细分/6-18-22-18-P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md` | 回写 capabilities、docs、matrices 和 remaining boundaries |
| source candidates | `矩阵/p8_marker_placement_source_candidates.tsv` | 20 条 FreeCAD / cad-core 候选证据，覆盖 dispatch、resolver、oracle、special regression、capability |
| scope review | `矩阵/p8_marker_placement_scope_review_matrix.tsv` | 14 个 scope 状态和验收路由 |
| blocker queue | `矩阵/p8_marker_placement_blocker_queue.tsv` | 10 个发布前必须关闭的 blocker |
| non goal registry | `矩阵/p8_marker_placement_non_goal_registry.tsv` | 不进入本轮实现的边界 |
| backend gap classification | `矩阵/p8_marker_placement_backend_gap_classification.tsv` | 12 类 backendGap / notCollected / releaseGate / nonGoal 聚合 |

当前本文已执行 S0-S1，S2-S6 尚未执行。矩阵是 evidence / route，不是 supported 结论；不得把 subshape marker placement、S2-S6、capability publication 或 oracle parity 提前标为 supported。
