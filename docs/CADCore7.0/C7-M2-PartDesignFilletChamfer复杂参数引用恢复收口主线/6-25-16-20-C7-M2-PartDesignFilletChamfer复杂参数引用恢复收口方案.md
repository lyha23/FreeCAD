# C7-M2 PartDesign Fillet Chamfer 复杂参数引用恢复收口方案

## 背景

C7-M1 已把 Hole ModelThread 与标准孔表边界收口到 release gate，队列为空。P7 PartDesign 常用生态文档曾保留一条 `Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复` 口径。基础 Fillet / Chamfer 并非空白能力：现有 fixtures/tests 已覆盖 Body-member native oracle、RefineModel、诊断型错误、SupportTransform 和链式 DressUp 被 transformed family 消费；C7-M2 S4 已把剩余口径拆成 inherited expected-backed、oracle pending 和 non-goal / publication-only。

C7-M2 因此必须按“最小完整语义批次”推进：同一 FreeCAD 调用链、同一 DressUp Base / AddSubShape / SupportTransform 边界、同一 P7 expected 家族一起裁决。只有 S2 证明存在 active backend gap，S3 才能改 C++ 或 fixtures。

## 目标

- 冻结当前 Fillet / Chamfer live baseline，避免把已支持能力误判为新任务。
- 复核 FreeCAD `FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDressUp.cpp` 的参数、选边、AddSubShape 和 SupportTransform 语义。
- 建立 complex parameter、multi-edge / UseAllEdges、Body/DressUp chain reference recovery、capability/docs publication 的矩阵。
- 裁决每个 row 的 route：already closed、oracle pending、backend gap、publication-only、non-goal。
- 对 S2 批准的 backend gap 实施正式分层修复；如果没有 backend gap，则做 no-code publication closure。

## 范围

### 必须纳入同一批次

- Fillet `Radius`、`UseAllEdges`、selected edge、continuous edge expansion、multi-edge。
- Chamfer `ChamferType`、`Size`、`Size2`、`Angle`、`FlipDirection`、`UseAllEdges`。
- DressUp Base LinkSub、Body cumulative shape、前序 Fillet/Chamfer Base、stable subname 输入和 diagnostic。
- `SupportTransform=true` 的 AddSubShape cache、slot ownership 和链式 DressUp source base。
- `cad-core/fixtures/p7` 现有 Fillet / Chamfer / SupportTransform fixtures 与 focused tests。

### 明确排除

- GUI Fillet / Chamfer task panel、交互选择器和 preview UI。
- Draft、Thickness 等非 Fillet / Chamfer dress-up 类型。
- full topo naming / full MapperHistory 泛化工程。
- 在输出端靠 fixture 名称、边顺序、source edge 猜测稳定引用。
- transformed family 超出 Fillet / Chamfer SupportTransform 链路的复杂参数全集。

## 步骤

### S0 live baseline

已完成。live 起点为 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=b5767e2391`（`b5767e2391 docs: 新增 C7-M2 Fillet Chamfer 收口方案`），开始时 `git status --short -uall` 无输出；C7-M1 队列为空，C7-M2 初始队列为 S0-S5 pending。本步只改 docs/matrices，不改 C++、fixtures、expected 或 tests。

S0 冻结的 supported baseline：P7 文档记录基础 Edge / Face Base、连续边过滤、OCCT fillet/chamfer maker、replacement solid、RefineModel、DressUp AddSubShape cache、slot 级 `NamedShape`、`SupportTransform=true` 和链式 DressUp transformed consumption 已覆盖；`capability_contract.cpp` 的 `producer_matrix.dressup` 为 `done_first_slice`，covered 包含 `addsubshape_slot`、`multi_selection_history`、`chamfer_parameter_variants`、`failure_diagnostics`、`chain_dressup_pattern_history`，remaining 为空。

S0 冻结的 P7 残余口径仍是 `Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`。代表 fixture / expected 为 `p7/fillet-pad-edge`、`p7/chamfer-pad-edge`、`p7/fillet-refine-true`、`p7/chamfer-refine-true`、`p7/mirrored-fillet-support-transform`、`p7/mirrored-dressup-chain-support-transform`；诊断 fixture 为 `p7/fillet-missing-edge`、`p7/chamfer-invalid-size`；相邻 C3-M5 证据为 `chamfer-two-distances-edge`、`chamfer-distance-angle-edge`、`fillet-face-selection-history`、`chained-dressup-pattern-history`。focused test names 以当前 `cad-core/tests/test_p7_features.py` 为准：`test_p7_fillet_replaces_body_tip_shape`、`test_p7_chamfer_replaces_body_tip_shape`、`test_c3m5_chamfer_parameter_variants_build`、`test_p7_dressup_refine_true_uses_refinemodel_path`、`test_p7_dressup_base_diagnostics_are_structured`、`test_c3m5_dressup_face_selection_records_expanded_edge_history`、`test_p7_mirrored_features_mode_consumes_dressup_support_transform_cache`、`test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache`、`test_c3m5_chained_dressup_pattern_history_keeps_support_transform_slot`。

### S1 FreeCAD 源码与 oracle 候选

阅读 FreeCAD 源码并更新 `source_candidates`、`input_contract`、`oracle_fixture` 矩阵。必须引用具体文件、类/函数和关键字段名，不能只写“参考 FreeCAD”。

S1 已完成。矩阵已记录 FreeCAD `Fillet::execute`、`Chamfer::execute/updateProperties/migrateFlippedProperties`、`DressUp::getContinuousEdges/getFaces/getAddSubShape`，并把 cad-core 落点拆为 `feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_dress_up.cpp` 与 `feature_transformed.cpp`。

### S2 准入裁决

已完成。live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=00c035d224`（`00c035d224 docs: 补齐 C7-M2 S1 源码与 oracle 矩阵`），开始时 `git status --short -uall` 无输出。

S2 route：

- `already_closed_expected_backed`
  - Chamfer Two distances：继承 `c3m5/chamfer-two-distances-edge` fixture/expected 与 `test_c3m5_chamfer_parameter_variants_build`。
  - Chamfer Distance and Angle：继承 `c3m5/chamfer-distance-angle-edge` fixture/expected 与 `test_c3m5_chamfer_parameter_variants_build`。
  - SupportTransform mirrored / chained DressUp regression：继承 p7 expected 与 `test_p7_mirrored_features_mode_consumes_*support_transform_cache`。
- `oracle_pending_collect`
  - Fillet multi-edge / `UseAllEdges`：FreeCAD/cad-core 均有执行路径，但缺 dedicated FreeCAD expected。
  - Chamfer `FlipDirection=true`：FreeCAD/cad-core 均有 true-side路径，但现有 expected 只覆盖 false。
  - DressUp chain stale `ReferenceShadow` / Base recovery：现有 evidence 只覆盖 Body cumulative Base、invalid stable diagnostic、SupportTransform chain source_base 和 transformed slot history，缺成功 stale recovery oracle。
- `backend_gap_requires_implementation`
  - 无。
- `publication_closure_only`
  - S2/S3/S4 只同步 route、oracle pending、non-goal 和 inherited expected-backed 发布口径。
- `diagnostic_non_goal`
  - GUI、full DressUp universe、full MapperHistory、输出端引用恢复猜测。

S2 没有 `backend_gap_requires_implementation`。Code edit gate 保持关闭，S3/S4 只能做 no-code diagnostic/publication closure，不改 C++、fixtures、expected 或 tests。

### S3 实现或 diagnostic 边界收口

已完成。live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5adeee22a3`（`5adeee22a3 文档：完成 C7-M2 S2 准入裁决`），开始时 `git status --short -uall` 无输出。

S3 未获得实现授权，因此没有修改 C++、fixtures、expected 或 tests，也没有新增测试。S3 只落实 no-code diagnostic/publication boundary：Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 保持 `oracle_pending_collect`；Chamfer Two distances、Chamfer Distance and Angle、SupportTransform regression 保持 `already_closed_expected_backed`；GUI、full DressUp universe、full MapperHistory 和输出端引用恢复猜测保持 `diagnostic_non_goal`；publication drift 保持 `publication_closure_only` 并进入 S4。

若后续 oracle 证明 active backend gap，涉及 FreeCAD 语义的 public API、executor 主路径、mapper/history 字段必须在相邻注释写明 FreeCAD 源文件、类/函数和关键短句；引用恢复必须优先补 `topo` / history / naming 正式能力，不允许在 adapter 或 executor 输出端修剪。

### S4 fixtures/tests/capability 发布

已完成。live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=934ffa6ff8`（`934ffa6ff8 文档：完成 C7-M2 S3 no-code 边界收口`），开始时 `git status --short -uall` 无输出。

S4 未改 C++、fixtures、expected、tests 或 `cad-core/src/runtime/capability_contract.cpp`。本轮只同步 root README、本包 README、总入口、方案、P7 文档和矩阵发布口径：

- Chamfer Two distances、Chamfer Distance and Angle、SupportTransform mirrored / chained DressUp regression 是 inherited `already_closed_expected_backed`。
- Fillet multi-edge / `UseAllEdges`、Chamfer `FlipDirection=true`、DressUp chain stale `ReferenceShadow` / Base recovery 是 `oracle_pending_collect`，必须先有后续 FreeCAD oracle package，不能发布为 supported capability。
- GUI、full DressUp universe、full MapperHistory 和 output-side stable reference guessing 是 `diagnostic_non_goal`。
- `publication_closure_only` 已关闭，下一步进入 S5 release gate；因为 S4 只改 docs/矩阵，不运行 C++ build/unittest。

### S5 release gate

清空队列，按代码变更实际范围运行 focused build/tests。仅当 C++、fixtures、expected、adapter schema 或 topo/history 广泛变动时才提升到重型阶段回归。

## 验收分层

### 本轮文档短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```

### 实现短跑

S3/S4 未改 C++、fixtures、expected 或 tests，所以不运行 focused unittest。S5 只有在后续实际改动代码、fixture 或 expected 发布口径时，才从当前 `cad-core/tests/test_p7_features.py` 读取真实 test names 后选择 focused filters。

### 阶段回归

S5 若触发代码或 expected 发布变更，默认至少运行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

## 收口标准

- S2 每个 row 都有 route、证据和下一步。
- active backend gap 不被标成 supported，unsupported/non-goal 不被写入 capability supported。
- fixtures/expected/tests/capability/docs 口径一致。
- 没有 fixture-specific 输出修正或 executor 端引用猜测。
- C7-M2 `工作步骤细分` 队列为空后，才允许声明本包完成。
