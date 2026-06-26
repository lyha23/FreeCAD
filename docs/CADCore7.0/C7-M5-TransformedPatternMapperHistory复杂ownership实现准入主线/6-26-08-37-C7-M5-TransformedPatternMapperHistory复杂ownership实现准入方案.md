# C7-M5 Transformed Pattern MapperHistory 复杂 ownership 实现准入方案

## 背景

旧 P7 Transformed 主线已经把 Mirrored、LinearPattern、PolarPattern、Scaled、MultiTransform 的基础 topology oracle、final-result refine、supported / covered 发布闸门收口。当前总览后续队列仍保留一个更深方向：P7 transformed / pattern 完整 MapperHistory 与更复杂 ownership。

C7-M5 的核心不是重新实现已经关闭的 topology_counts，而是审计更复杂 ownership 是否真的有 FreeCAD native evidence 和当前 `cad-core` mismatch。没有 source-backed native oracle 时，只能保持 `oracle_pending` / `oracle_blocked` / `diagnostic_non_goal`，不能直接改 C++。

## 目标

- 从旧 P7 Transformed 主线、P7 live 文档、FreeCAD source 和当前 `cad-core` tests 中抽出复杂 ownership 候选。
- 形成最小完整语义批次：source authority、fixture / expected 候选、collector 命令、focused tests、capability / docs 发布口径。
- 若 native oracle 证明 current `cad-core` mismatch，打开 S5 implementation gate。
- 若 current `cad-core` 已匹配，发布 `already_closed_expected_backed`。
- 若缺 oracle 或生命周期不可复现，发布 `oracle_blocked` 或 `diagnostic_non_goal`，不改 C++。

## 最小完整语义批次

| 批次 | 代表项 | 判定方式 |
| --- | --- | --- |
| Transformed copy ownership | `TransformN.*` alias、original stable alias、source retag、terminal split / deleted history | FreeCAD `makeElementTransform()` / ElementMap copy authority + focused P7 history tests |
| Pattern slot ownership | chained DressUp `SupportTransform`、multi-original add / sub replay、AddSubShape slot owner | FreeCAD `DressUp::getAddSubShape()` + `Transformed::execute()` AddSub replay |
| MultiTransform composition | child template order、Scaled diagonal、non-Scaled multiplication、fallback diagnostic | FreeCAD `MultiTransform::getTransformations()` + checked-in expected |
| Whole shape support owner | Body prefix support、refined prefix support、support-backed Whole shape lifecycle | native fixture must include Body / BaseFeature lifecycle, not standalone geometry-equivalent |

## 实施纪律

- S0/S1 不改 C++、fixtures、expected 或 tests；只冻结状态、source 和当前 coverage。
- S2 只输出 oracle 候选和最小批次；不能把 old P7 supported row 误写成 active gap。
- S3 可以新增 oracle fixture / expected / known_gap，但不能从 current `cad-core` 输出倒推 expected。
- S4 只做 parity 和 gate 裁决；只有 route=`backend_gap_requires_implementation` 才打开 S5 code edit gate。
- S5 若实现，必须落到正式 `features/transformed` / `features/dress_up` / `topo` 路径，不允许 adapter 修正、fixture 名称分支、source index / transform index 猜测或输出排序特判。

## 步骤

### S0 live baseline 与旧 P7 边界冻结

冻结当前 live 起点、C7-M1..M4 队列、旧 P7 Transformed S0-S6 completion、P7 live 口径和总览后续队列。S0 不采 oracle、不改代码。

S0 已完成：live 起点 `HEAD=a2cc93a1ee`（`a2cc93a1ee 文档：收口 C7-M5 工作步骤总入口索引`），开始状态干净；C7-M1/C7-M2/C7-M3/C7-M4 和旧 P7 Transformed 队列为空。旧 P7T `P7T-SCOPE-001..007` 保持 `supported`，`P7T-BG-001/002` 保持 supported/covered closed，`P7T-BG-003` 保持 standalone lifecycle boundary，`P7T-BLOCK-001..005` 均 closed，`P7T-NG-005` 保持 standalone Whole shape nonGoal，冻结表写入 `矩阵/c7m5_transformed_history_p7_boundary_freeze.tsv`。S0 未采 oracle、未运行 FreeCADCmd、未新增或修改 fixtures/expected/tests、未改 C++；队列推进到 S1。

### S1 FreeCAD source 与 current coverage 复核

复核 FreeCAD source authority：

- `src/Mod/PartDesign/App/FeatureTransformed.cpp::Transformed::execute()`
- `src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `src/Mod/PartDesign/App/FeatureScaled.cpp`
- `src/Mod/PartDesign/App/FeatureMultiTransform.cpp`
- `src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()`
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementTransform()`

同时复核 `cad-core/src/part_design/feature_transformed.cpp`、各子 feature、`cad-core/src/part/topo_shape.cpp`、`cad-core/tests/test_p7_features.py` 和 relevant expected。

S1 已完成：live 起点为 `HEAD=27b2f84d6a`（`27b2f84d6a docs: 完成 C7-M5 S0 基线冻结`），开始状态干净；本轮未采 FreeCAD oracle、未新增或修改 fixtures/expected/tests、未改 C++。复核结论如下：

- FreeCAD `Transformed::execute()` 的主顺序是跳过 MultiTransform child、必要时由 Body 写入 BaseFeature、`positionBySupport()`、调用子类 `getTransformations()`，再在 Features 模式中对每个 Original 单独 `getAddSubShape()`、`makeElementTransform()`、`makeElementFuse()` / `makeElementCut()`；WholeShape 模式把 support 本体和 transformed support copies 一起 fuse，最后再 `refineShapeIfActive()`。
- FreeCAD `DressUp::getAddSubShape()` 的 `SupportTransform` 会跳过连续 DressUp 找到前一个 `FeatureAddSub` owner；additive support 只生成 add slot，subtractive support 只生成 sub slot；非 `SupportTransform` 路径生成 add/sub 两个 cut slot。
- FreeCAD `MultiTransform::getTransformations()` 已确认 child order、Scaled diagonal divisor、非 Scaled multiplication、first-original COG 和 WholeShape 空 originals 时默认 origin 的行为；Mirrored / LinearPattern / PolarPattern / Scaled 的 transform list source 已复核。
- FreeCAD `TopoShape::makeElementTransform()` 的 ownership authority 是 transform/move 后 `copyElementMap(tmp, op)`，不是根据输出几何猜测 source owner。
- current `cad-core` 已有 `namedShapeForTransformedCopy()` source alias / original stable alias / nested history / merge history 路径；P7 tests 已覆盖 `TransformN` aliases、terminal split/deleted diagnostics、`history_consumed:merge`、DressUp slot history、MultiTransform linear+mirror、Scaled diagonal、divisor diagnostic 和 whole-shape support。
- S2 输入池已写入矩阵：`C7M5-SCOPE-101` 为 `already_covered_baseline`；`C7M5-SCOPE-201` 和 `C7M5-SCOPE-301` 为 `s2_oracle_candidate_input_pool`；`C7M5-SCOPE-401` 为 `diagnostic_non_goal_baseline`。S2 只能基于 support-backed lifecycle 决定 oracle candidate，不得把 S0 冻结的旧 P7T closed baseline 改成 backendGap。

### S2 oracle 候选矩阵与批次裁决

把候选路由到：

- `already_covered`
- `oracle_candidate`
- `oracle_blocker`
- `backend_gap_candidate`
- `diagnostic_non_goal`

S2 必须明确每个候选是否有 support-backed FreeCAD lifecycle，是否已有 checked-in expected，是否只是 geometry-equivalent smoke。

S2 已完成：live 起点为 `HEAD=cbfbfe736d`（`cbfbfe736d docs: 完成 C7-M5 S1 源码与覆盖复核`），开始状态干净；本轮未采 FreeCAD oracle、未新增或修改 fixtures/expected/tests、未改 C++。裁决结论如下：

- `C7M5-SCOPE-101` route=`already_covered`：TransformN alias、original stable alias、terminal split/deleted history、merge history 和 `element_history_status` 已由现有 P7 focused tests 与 expected 覆盖，S3 不再采新 oracle。
- `C7M5-SCOPE-201` route=`oracle_candidate`：以 support-backed `mirrored-dressup-chain-support-transform` 和 `linear-pattern-pad-pocket-multi-original` 作为 Pattern AddSubShape slot ownership 的最小完整语义批次；S3 只做 native expected / blocker / evidence 固化。
- `C7M5-SCOPE-301` route=`oracle_candidate`：以 support-backed `multi-transform-linear-mirror` 和 `multi-transform-scaled-diagonal` 作为 MultiTransform composition ownership 的最小完整语义批次；`multi-transform-whole-shape` 这类 standalone smoke 不升格为 native golden。
- `C7M5-SCOPE-401` route=`diagnostic_non_goal`：standalone Whole shape lifecycle boundary 延续旧 P7T `P7T-NG-005`，缺 Body/BaseFeature lifecycle 的 geometry-equivalent case 不得变成 backendGap 或 native ownership oracle。
- S2 没有打开任何 `backend_gap_candidate`；只有 S4 在 S3 native oracle 证明 current `cad-core` mismatch 后才允许裁决 implementation gate。

### S3 native oracle 采集

对 S2 的 `oracle_candidate` 批次采集 FreeCAD expected。合法结果：

| route | 条件 | 输出 |
| --- | --- | --- |
| `native_oracle_collected` | FreeCAD native fixture 可复现 ownership / history evidence | expected / evidence JSON |
| `native_oracle_blocked` | collector、FreeCADCmd 或 lifecycle 不可观察 | known_gap JSON |
| `diagnostic_non_goal` | standalone / GUI / session / unsupported child type 等超边界 | diagnostic expected 或 docs row |

S3 已完成：live 起点为 `HEAD=d8ad940e33`（`d8ad940e33 docs: 完成 C7-M5 S2 oracle 候选裁决`），开始状态干净；本轮只运行 native oracle check、更新文档和矩阵，未改 C++、runtime、adapter、expected 或 tests。执行命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py fixtures/p7/mirrored-dressup-chain-support-transform.json --check
python3 tools/collect_freecad_expected.py fixtures/p7/linear-pattern-pad-pocket-multi-original.json --check
python3 tools/collect_freecad_expected.py fixtures/p7/multi-transform-linear-mirror.json --check
python3 tools/collect_freecad_expected.py fixtures/p7/multi-transform-scaled-diagonal.json --check
```

四个 check 均通过，runtime 输出 `FreeCAD 1.2.0, Libs: 1.2.0devR20260519 (Git shallow)`，checked-in expected 记录 `freecad_version=1.2.0 revision 20260519`。`C7M5-ORACLE-201` 固化 Pattern AddSubShape slot ownership 的 topology 与 checked-in `named_shapes` evidence：`mirrored-dressup-chain-support-transform` 覆盖 `Chamfer/Fillet/Pad/SketchPad` prefixes 和 split/deleted/merge history，`linear-pattern-pad-pocket-multi-original` 覆盖 `Pad/Pocket/SketchPad/SketchPocket/LinearPattern.Transform1` prefixes。`C7M5-ORACLE-301` 固化 MultiTransform composition：`multi-transform-linear-mirror` 覆盖 `MultiTransform.Transform` prefixes 与 `faces=12, edges=24, vertices=16`，`multi-transform-scaled-diagonal` 覆盖 diagonal composition topology `faces=18, edges=36, vertices=24`、bbox `[0.0, -0.5, -0.5]..[7.5, 1.5, 1.5]`、volume `12.375`。collector `--check` 只重采 FreeCAD geometry payload；`named_shapes` ownership/history evidence 由 checked-in expected 与 focused P7 tests 消费。`C7M5-ORACLE-401` 保持 `diagnostic_non_goal`，standalone Whole shape 仍不采 native golden。

### S4 cad-core parity 与 implementation gate

如果 S3 得到 native oracle，则比较 current `cad-core`：

- 匹配：`already_closed_expected_backed`
- 不匹配：`backend_gap_requires_implementation`
- 缺 oracle：`oracle_blocked`
- 超边界：`diagnostic_non_goal`

S4 必须写清 S5 是否允许改 C++、允许文件范围、focused tests 和 non-goals。

S4 已完成：live 起点为 `HEAD=4baa80c37a`（`4baa80c37a docs: 完成 C7-M5 S3 native oracle 固化`），开始状态干净；本轮只运行 parity unittest、更新文档和矩阵，未改 C++、fixtures、expected 或 tests。执行命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_mirrored_features_mode_consumes_chained_dressup_support_transform_cache tests.test_p7_features.CadCoreP7FeatureTest.test_p7_linear_pattern_replays_multi_original_add_and_sub_slots tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_combines_linear_pattern_and_mirror tests.test_p7_features.CadCoreP7FeatureTest.test_p7_multi_transform_scaled_child_uses_diagonal_composition tests.test_p7_features.CadCoreP7FeatureTest.test_p7_transformed_copy_preserves_terminal_stable_history
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

结果：focused 5 项通过（`Ran 5 tests in 1.601s`），完整 `CadCoreP7FeatureTest` 148 项通过（`Ran 148 tests in 27.855s`）。这些测试覆盖 S3 四个 support-backed expected 的 topology、diagnostics、ElementMap / `named_shapes`、`element_history_status`、TransformN alias、SupportTransform slot ownership、MultiTransform composition 和 expected fixture assertions。裁决为 `already_closed_expected_backed`：`C7M5-ORACLE-201` / `C7M5-ORACLE-301` 不打开 `backend_gap_requires_implementation`；`C7M5-ORACLE-101` 继续作为 already covered regression guard；`C7M5-ORACLE-401` 保持 `diagnostic_non_goal` carry-forward。S5 不允许改 C++，只做 no-code publication closure；若后续要重开实现，必须先有新的 source-backed native oracle mismatch 或 upstream scope change。

### S5 实现或 no-code 发布

若 S4 打开 code gate，S5 实现顺序固定：

1. 在 transformed / dress_up / topo 正式路径补 ownership / MapperHistory 账本。
2. 写 focused tests，约束 geometry、ElementMap、`element_history_status`、stable diagnostics 和 capability / docs 口径。
3. 删除旧 fallback 或保持 known_gap 时同步矩阵。

若 S4 未打开 code gate，S5 只做 no-code publication closure。

S4 当前裁决未打开 code gate，因此 S5 范围限定为 no-code publication closure：允许更新本包 README、总入口、方案、矩阵和必要的 P7 发布口径；不允许修改 `cad-core/src/part_design`、`cad-core/src/part`、fixtures、expected、tests 或 adapter。focused tests 只作为回归记录，不要求重新 build。

S5 已完成：live 起点为 `HEAD=77b7903f76`（`77b7903f76 docs: 完成 C7-M5 S4 parity gate 裁决`），开始状态干净；本轮执行 no-code publication closure，关闭 `C7M5-BLOCKER-501`，发布 C7-M5 expected-backed closed / no backendGap。S5 只更新 README、主线总入口、方案、矩阵和必要的 P7 live / 总览后续口径，未修改 C++、fixtures、expected、tests、adapter、collector 或 capability，未运行 build 或 unittest；S4 focused 5 tests 与完整 `CadCoreP7FeatureTest` 通过结果作为发布依据。standalone geometry-equivalent Whole shape 保持 diagnostic non-goal，不升格为 native golden；旧 P7T rows 不重开。队列推进到 S6。

### S6 release gate

运行本包 queue、TSV、trailing whitespace、`git diff --check`。若 S5 改 C++，再跑 focused P7 tests 和 `cmake --build build`。

## 验收分层

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 docs/CADCore7.0/README.md
git diff --check
```

### 实现短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

只有 S5 改 C++、expected、tests 或 capability 时，这组才是必须执行项。
