# 【已实现】P8 Assembly Reference / JCS MarkerPlacement S4 NativeOracle 与代表 fixture 专项复审

## 目标

为同一 marker placement 语义批量采集 representative FreeCAD expected，不再只挑单个 Distance fixture。

S4 只采集 oracle / expected 与测试路由，不声明 cad-core 已完成 subshape marker parity。

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
75267044ae

git log -1 --oneline
75267044ae feat: 完成 P8 MarkerPlacement S3 resolver 前置

git -c core.quotepath=false status --short -uall
<clean>

/Users/li/.cargo/bin/FreeCADCmd --version
FreeCAD 1.2.0 Revision: 20260519 (Git shallow)
```

## 输入复核

- S0-S3 已实现文档确认当前 `cad-core` 只完成 resolver evidence / diagnostic 前置；subshape markerPlacement 仍由 S5 负责。
- `p8_marker_placement_scope_review_matrix.tsv` 中 `MP-SCOPE-005/006/007/009/012` 是本轮 oracle 采集目标。
- `p8_marker_placement_blocker_queue.tsv` 中 `MP-BLOCK-004` 由 S4 关闭；`MP-BLOCK-005` 的 mixed/current-value oracle 部分由 S4 关闭，focused parity 留给 S5。
- `cad-core/tools/collect_freecad_expected.py` 原本只保留 solver adapter / placement_updates；S4 补充 native marker/current-value oracle evidence。

## collector 变更

- 新增 `native_marker_oracle` expected 字段，来源标注到：
  - `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`
  - `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/App/AssemblyUtils.cpp::getJointCurrentValue()`
  - `/Users/li/Chili3DProject/FreeCAD/src/Mod/Assembly/UtilsAssembly.py::getJointDistance()` / `getJointXYAngle()`
- 对 subshape marker oracle 自动写入顶层 `known_gap` 与 `backendGap.ids=["MP-BLOCK-002","MP-BLOCK-003","MP-BLOCK-006"]`。
- `known_gap` 删除条件：S5 实现 FreeCAD `handleOneSideOfJoint()` object/subshape global -> moving-part local marker resolver，并让本批 expected 的 `placement_updates` focused parity 通过。
- `native_marker_oracle` 不放进 `solver_adapter` 比对字段；它是 native oracle 证据，不让当前 S3 后端误判为 supported。

## checked-in oracle 批次

| fixture 组 | 覆盖 |
| --- | --- |
| object baseline | 既有 object-level native placement expected 保持不刷新；继续作为 `MP-SCOPE-001` supportedBaseline |
| vertex | 新增 `assembly-marker-ball-vertex-real-solver`；更新 `assembly-distance-point-point-zero-real-solver`、`assembly-distance-point-point-nonzero-real-solver` |
| edge | 新增 `assembly-marker-revolute-edge-real-solver`、`assembly-marker-slider-edge-real-solver`、`assembly-marker-cylindrical-edge-real-solver`；更新 `assembly-distance-line-line-real-solver`、`assembly-distance-point-line-real-solver` |
| face | 新增 `assembly-marker-fixed-face-real-solver`、`assembly-marker-parallel-face-real-solver`、`assembly-marker-perpendicular-face-real-solver`、`assembly-marker-angle-face-real-solver`；更新 `assembly-distance-plane-plane-real-solver`、`assembly-distance-point-plane-real-solver`、`assembly-distance-line-plane-real-solver` |
| mixed + swapped | `PointLine`、`PointPlane`、`LinePlane` expected 保留 `jcs_swapped_for_solver=true` 和 solver-side reference order |
| current value | 7 个 DistanceType expected 写入 `linear_distance_from_jcs_placements`；Angle face expected 写入 `xy_angle_radians_from_jcs_placements` |
| special rewrite | 新增 `assembly-rackpinion-marker-rewrite-real-solver.freecad.json`；既有 Screw / RackPinion / Gears / Belt expected 继续作为 S5 regression route |

## S4 状态裁决

- `MP-BLOCK-004` 已由 S4 关闭：Vertex / Edge / Face primitive native oracle batch 已入库并逐 fixture `--check` 通过。
- `MP-BLOCK-005` oracle 部分已完成：mixed swap 和 current-value evidence 已入库；S5 仍需 focused parity 关闭 backendGap。
- `MP-SCOPE-005/006/007/009/012` 从 `notCollected` 改为 `oracleCollected`；不是 `supported`。
- 15 个 subshape expected 均保留精确 `known_gap`，让 `CadCoreExpectedFixtureTest` 跳过，避免把 S3 的 subshape diagnostic / identity fallback 写成 parity。
- `assembly-rackpinion-marker-rewrite-real-solver.freecad.json` 不带 `known_gap`，用于保护 RackPinion marker rewrite regression。
- 未刷新 unrelated c3m6 expected；未发布 C ABI capability；未实现 S5 resolver parity；未实现 radius-bearing / curve DistanceType。

## 验收

```bash
python3 cad-core/tools/collect_freecad_expected.py cad-core/fixtures/c3m6/<fixture>.json --out cad-core/fixtures/c3m6/expected/<fixture>.freecad.json --check --freecadcmd /Users/li/.cargo/bin/FreeCADCmd
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k marker
python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
git diff --check -- cad-core/fixtures/c3m6 cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

本轮验证结论：

- 16 个 S4 相关 fixture 已分别执行单项 `collect_freecad_expected.py --check --freecadcmd /Users/li/.cargo/bin/FreeCADCmd`，均通过。
- `python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch` 通过。
- `python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest` 通过，结果 `OK (skipped=24)`；跳过项包含本轮带 `known_gap` 的 subshape oracle expected。
- FreeCADCmd 输出中出现的 redundant constraint / convergence 文本来自 native solver 采集过程，不作为 cad-core parity 结论。

## 非目标

- 不刷新 unrelated c3m6 expected。
- 不把 expected mismatch 归因到 FreeCAD 前先核对 fixture 构造。
- 不以一个 primitive 或一个 Distance fixture 代表整个 marker placement 语义。
- 不删除 `known_gap`；删除动作属于 S5 focused parity。
- 不发布 C ABI capability。
