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

### S2 oracle 候选矩阵与批次裁决

把候选路由到：

- `already_covered`
- `oracle_candidate`
- `oracle_blocker`
- `backend_gap_candidate`
- `diagnostic_non_goal`

S2 必须明确每个候选是否有 support-backed FreeCAD lifecycle，是否已有 checked-in expected，是否只是 geometry-equivalent smoke。

### S3 native oracle 采集

对 S2 的 `oracle_candidate` 批次采集 FreeCAD expected。合法结果：

| route | 条件 | 输出 |
| --- | --- | --- |
| `native_oracle_collected` | FreeCAD native fixture 可复现 ownership / history evidence | expected / evidence JSON |
| `native_oracle_blocked` | collector、FreeCADCmd 或 lifecycle 不可观察 | known_gap JSON |
| `diagnostic_non_goal` | standalone / GUI / session / unsupported child type 等超边界 | diagnostic expected 或 docs row |

### S4 cad-core parity 与 implementation gate

如果 S3 得到 native oracle，则比较 current `cad-core`：

- 匹配：`already_closed_expected_backed`
- 不匹配：`backend_gap_requires_implementation`
- 缺 oracle：`oracle_blocked`
- 超边界：`diagnostic_non_goal`

S4 必须写清 S5 是否允许改 C++、允许文件范围、focused tests 和 non-goals。

### S5 实现或 no-code 发布

若 S4 打开 code gate，S5 实现顺序固定：

1. 在 transformed / dress_up / topo 正式路径补 ownership / MapperHistory 账本。
2. 写 focused tests，约束 geometry、ElementMap、`element_history_status`、stable diagnostics 和 capability / docs 口径。
3. 删除旧 fallback 或保持 known_gap 时同步矩阵。

若 S4 未打开 code gate，S5 只做 no-code publication closure。

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
