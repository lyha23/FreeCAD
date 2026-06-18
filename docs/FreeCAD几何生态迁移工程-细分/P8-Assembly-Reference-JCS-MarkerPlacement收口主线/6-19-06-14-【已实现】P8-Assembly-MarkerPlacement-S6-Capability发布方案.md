# 【已实现】P8 Assembly MarkerPlacement S6 Capability 发布方案

本文是 `P8-Assembly-Reference-JCS-MarkerPlacement收口主线` 包内的 S6 发布闸门实施方案和收口记录，组织方式对齐 `P8-ScrewRackPinionJoint-OndselSolver收口主线`：总入口、工作步骤、矩阵继续作为主索引，本文件只承接 capability / docs / matrix 发布实施边界。

## 当前基线

- 当前仓库：`/Users/li/Chili3DProject/FreeCAD`，S5 已提交 `a353384bae fix: 关闭 P8 Assembly PointLine placement gap`，S6 worker 在 `a353384bae` 上执行发布闸门。
- live 队列已通过 S6 收口；S6 步骤文件已改名为 `P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-18-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md`。
- `CadCoreExpectedFixtureTest` 中 S5 代表 expected 已激活，PointLine 不再带 `known_gap` / `backendGap`；本阶段不重新采 oracle，不修改 expected。
- `cad-core/src/adapters/c_api/c_api.cpp::ondselSolverCapabilityJson()` 已发布 S5 证明的 `subshape_marker_placement` representative subset，并保留未证明边界为 non-goal。
- `cad-core/tests/test_adapters.py::CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts()` 已检查 subshape marker placement 发布字段、active expected 数、non-goals 和空 remaining gaps。

## 目标

S6 只做发布闸门：把 S3-S5 已证明的 Reference / JCS subshape marker placement 能力写入 C ABI capability、adapter tests 和 P8 文档矩阵，并保持未证明边界不被误发布。

本阶段完成后，P8 MarkerPlacement 包应达到：

- capability JSON 显式包含 `subshape_marker_placement` 或等价字段。
- supported subset 只声明已由 S4/S5 expected 和 focused tests 证明的范围。
- `MP-BLOCK-008` capability publication sync 和 `MP-BLOCK-009` boundary protection 有明确关闭证据。
- goal-step queue 对该包不再留下可执行 S6 blocker。

## 证明链条

```text
S5 focused parity / active expected
  -> C ABI capability 发布字段
  -> adapter capability test
  -> blocker / backend gap / scope matrix 发布同步
  -> S6 工作步骤改名为【已实现】
  -> goal-step queue 出清
```

## 包内产物索引

| 类型 | 路径 | 本轮用途 |
| --- | --- | --- |
| 收口主线总入口 | `6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement收口主线总入口.md` | 回写 S0-S6 已收口和 supported subset 口径 |
| 工作步骤总入口 | `工作步骤细分/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement工作步骤总入口.md` | 将 S6 从待执行改为已实现，并证明队列出清 |
| S6 发布闸门 | `工作步骤细分/6-18-22-18-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md` | 已改名为 `【已实现】`，记录 capability 字段和验收结果 |
| source candidates | `矩阵/p8_marker_placement_source_candidates.tsv` | 保持 source authority，不因 S6 扩大语义 |
| scope review | `矩阵/p8_marker_placement_scope_review_matrix.tsv` | 发布 `MP-SCOPE-013`，保护 `MP-SCOPE-014` nonGoal |
| blocker queue | `矩阵/p8_marker_placement_blocker_queue.tsv` | 关闭 `MP-BLOCK-008/009`，确认 `MP-BLOCK-001..010` 均有 closed 结论 |
| backend gap classification | `矩阵/p8_marker_placement_backend_gap_classification.tsv` | 将 publication gate / boundary audit 改为 S6 已处理 |
| non goal registry | `矩阵/p8_marker_placement_non_goal_registry.tsv` | 确认 radius / curve / GUI / persistent solver / connector-only shortcut 仍不发布 |
| capability code | `cad-core/src/adapters/c_api/c_api.cpp` | 发布 `assembly.ondsel_solver_adapter.subshape_marker_placement` |
| adapter test | `cad-core/tests/test_adapters.py` | 固化 capability contract |

## 发布范围

允许发布的 supported subset：

- object-level marker placement baseline：已有 P8 AssemblySolver request-local object-level placement 支持，只作为 baseline，不替代 subshape 语义。
- Vertex marker placement：Ball Vertex、Distance PointPoint zero / nonzero active expected。
- Edge marker placement：Revolute、Slider、Cylindrical、Distance LineLine、Distance PointLine active expected。
- Face marker placement：Fixed、Parallel、Perpendicular、Angle、Distance PlanePlane、Distance PointPlane、Distance LinePlane active expected。
- mixed reference / request-local swap sync：PointLine、PointPlane、LinePlane 已通过 solver DTO、resolved marker evidence 和 native placement writeback 验证。
- real Ondsel consumption：`addConstraintToOndselAssembly()` 消费 resolved `markerPlacement`，15 个 active expected 通过真实 solver path。

禁止发布的范围：

- radius-bearing DistanceType：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`。
- curve/default DistanceType：`PointCurve`、`CurvePlane`、`Other`。
- GUI drag、postDrag、Reverse UI、跨请求 persistent solver state。
- connector-only subshape marker shortcut；object-level baseline 可以继续存在，但不能被写成 subshape support。
- 非 identity `offsetPlc` / bundled part 的完整泛化支持；当前只发布 S5 已验证的 request-local identity-offset AssemblyLink subset。

## 实施步骤

### 1. C ABI capability

修改 `cad-core/src/adapters/c_api/c_api.cpp`：

- 在 `ondselSolverCapabilityJson()` 的 `covered` 增加 `subshape_marker_placement`。
- 增加 `subshape_marker_placement` 对象，建议字段：
  - `status`: `covered_representative_subset`
  - `mode`: `request_local_handleOneSide_markerPlacement`
  - `build_mode`: `CAD_CORE_HAS_ONDSEL_SOLVER=1`
  - `supported_reference_kinds`: `object`, `Vertex`, `Edge`, `Face`, `mixed`
  - `covered`: `object_level_baseline`, `vertex_jcs_marker`, `edge_jcs_marker`, `face_jcs_marker`, `mixed_swap_marker_sync`, `real_ondsel_marker_consumption`, `placement_updates_native_parity`
  - `active_expected_count`: `15`
  - `active_expected_groups`: `S4/S5 c3m6 native marker expected`
  - `request_local_boundaries`: `identity_offset_assembly_link_subset`, `request_graph_no_persistent_solver_state`
  - `non_goals`: radius-bearing、curve/default、GUI/session、persistent solver state、connector-only subshape shortcut、non-identity bundled offsetPlc
  - `remaining_gaps`: 空数组；未发布项放入 `non_goals` 或 `request_local_boundaries`，不要写成 backend gap。
- 同步 `distance_type_basic_geometry.solver_joint_classes.PointLine`：若当前字段仍写 `ASMTCylSphJoint`，需要调整为能表达当前 native parity 路径的发布口径，例如 `["ASMTLineInPlaneJoint"]` 或 `{"native_marker_parity": "ASMTLineInPlaneJoint", "source_switch_legacy": "ASMTCylSphJoint"}`。不要让 capability 与 `collect_freecad_expected.py` / active expected 冲突。

### 2. Adapter capability tests

修改 `cad-core/tests/test_adapters.py`：

- 在 `test_c_api_capabilities_exposes_web_contract_facts()` 中断言：
  - `subshape_marker_placement` 出现在 `capabilities["assembly"]["ondsel_solver_adapter"]["covered"]`。
  - `capabilities["assembly"]["ondsel_solver_adapter"]["subshape_marker_placement"]["status"]` 为代表子集已覆盖状态。
  - supported reference kinds 包含 `object`、`Vertex`、`Edge`、`Face`、`mixed`。
  - `active_expected_count == 15`。
  - non-goals 包含 `radius_bearing_distance_type`、`curve_default_distance_type`、`GUI/session`、`persistent_solver_state`、`connector_only_subshape_marker_shortcut`。
  - `remaining_gaps == []`。
- 保留 existing DistanceType basic geometry 断言；若 PointLine capability class 改为 native parity 口径，同步更新测试。

### 3. 文档与矩阵

更新以下文件：

- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-18-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md`
  - 补充最终发布字段、supported subset、non-goal 边界、验收结果。
  - 实现并验证后按仓库规则重命名为 `6-18-22-18-【已实现】P8-Assembly-Reference-JCS-MarkerPlacement-S6-Capability与发布闸门.md`。
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement工作步骤总入口.md`
  - 将 S6 从待执行改为已实现。
  - 当前步骤队列应不再剩 S6。
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/6-18-22-11-P8-Assembly-Reference-JCS-MarkerPlacement收口主线总入口.md`
  - 明确 S0-S6 已收口，S6 只发布代表子集，不扩 radius / curve / GUI / persistent solver state。
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_blocker_queue.tsv`
  - `MP-BLOCK-008` 改为 capability/docs 已同步关闭。
  - `MP-BLOCK-009` 改为 boundary protection 已同步关闭。
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_scope_review_matrix.tsv`
  - `MP-SCOPE-013` 从 `releaseGate` 调整为已发布的代表子集状态。
  - `MP-SCOPE-014` 继续保持 `nonGoal`，但 close condition 写成 capability 已排除。
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/p8_marker_placement_backend_gap_classification.tsv`
  - `MP-BG-011` 改为 S6 已发布。
  - `MP-BG-012` 改为 S6 已审计排除。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 -m unittest cad-core.tests.test_adapters.CadCoreAdapterTest -k capabilities
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/矩阵/*.tsv
rg -n 'subshape_marker_placement|radius-bearing|curve/default|persistent_solver_state|connector_only_subshape_marker_shortcut' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
rg -n 'MP-BLOCK-00[1-9]|MP-BLOCK-010|connector_only_marker_shortcut' docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线 cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k assembly -k distance_type -k marker
python3 -m unittest cad-core.tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

重型收口只在 S6 发布前需要：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
```

## 完成标准

- `cad_core_capabilities_json()` 输出中有清晰的 `assembly.ondsel_solver_adapter.subshape_marker_placement` 发布字段。
- Adapter capability test 覆盖 supported subset、non-goal boundary 和空 remaining gaps。
- `MP-BLOCK-001..010` 在当前 blocker queue 中均有 closed 结论；`MP-BLOCK-008/009` 不再 pending。
- step queue 不再把 S6 列为 pending。
- 没有把 radius-bearing、curve/default、GUI/session、persistent solver state、connector-only subshape shortcut 写成 supported。
- 工作区只包含 S6 capability / docs / matrix / test 相关改动，并通过 `git diff --check`。

## 不做事项

- 不重新采集 FreeCAD expected。
- 不修改 `cad-core/fixtures/c3m6/expected`。
- 不扩大 JointType 支持范围。
- 不实现 radius-bearing 或 curve/default DistanceType。
- 不把历史 S2/S3/S4 文档批量改成当前状态；只更新总入口、S6 和当前矩阵。
