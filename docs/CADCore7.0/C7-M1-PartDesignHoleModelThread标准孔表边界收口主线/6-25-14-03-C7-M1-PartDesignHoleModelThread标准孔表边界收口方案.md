# C7-M1 PartDesign Hole ModelThread 标准孔表边界收口方案

## 背景

C6-M9 已关闭 Groove UpTo native failure 包，当前不应继续在 C6 里开薄的 Conic 包。Conic 剩余项主要是 GUI conic edit、full sketch solver conic constraints、DistanceType default/todo，不是当前后端 code-bearing 批次。

Hole 看起来在旧总览里仍写着 “ModelThread、标准件表驱动头部尺寸”，但 live 代码已经推进过一轮：`cad-core/src/runtime/capability_contract.cpp` 里 `part_design.hole.remaining_gaps=[]`，`model_thread.status=done_first_slice`；`cad-core/src/part_design/feature_hole.cpp` 已有 thread table、standard head cut、ModelThread pipe-shell tool 和 history freeze。C7-M1 因此不能写成泛 Hole 实现，而要写成边界收口：用批量 oracle 和 fixture family 复核发布状态，把 active expected-backed rows、legacy historical/non-active rows、capability/tests/docs 口径清干净。

## 最小完整语义批次

C7-M1 必须一轮覆盖同一批次，不拆成单 fixture：

- 同一 FreeCAD 调用链：`Hole::updateHoleCutParams()` / `determineDiameter()` / `execute()` / `makeThread()` / `findHoles()`。
- 同一 DTO/API 边界：PartDesign Hole object properties、`part_design.hole` capability、adapter metadata、diagnostics。
- 同一 expected family：`cad-core/fixtures/p7/hole-*.json` 与 `cad-core/fixtures/p7/expected/hole-*.freecad.json`。
- 同一验收闭环：批量采集 oracle、补 `cad-core` / history 实现、补 fixtures、补 focused tests、更新 capability/docs、写 release gate。

只有 S2 证明存在 FreeCAD 调用链分叉、oracle 无法采集、语义边界不清或风险会跨模块扩散时，才允许拆分；拆分必须写入矩阵，并列出下一批范围。

## 范围

### 必纳入

- ModelThread metric representative：`hole-supported-model-thread-metric` / `hole-model-thread-metric`。
- ModelThread + head cut representative：`hole-supported-model-thread-counterbore`。
- 标准孔表 head cut representative：`hole-supported-threaded-standard-counterbore`、`hole-supported-threaded-standard-countersink`、`hole-supported-threaded-dynamic-iso2009`、`hole-supported-threaded-dynamic-din7984`。
- 旧 pending rows：`hole-threaded-standard-counterbore`、`hole-threaded-standard-countersink`、`hole-threaded-dynamic-iso2009`、`hole-threaded-dynamic-din7984`。
- Profile source representative：`hole-point-profile`、`hole-supported-point-counterbore`，并保留 circle/arc baseline。
- Capability rows：`part_design.hole.model_thread`、`part_design.hole.history.covered`、`native_oracle_fixtures`、`remaining_gaps`。

### 可裁决后纳入

- 如果 S1 发现 `hole-threaded-known-gap` 仍代表真实 active gap，S2 必须给出迁移、补 oracle 或 historical route。
- 如果 S1/S2 发现 local-frame / topology mismatch 只影响 order 而几何等价，可归为命名顺序差异，不进入 backend gap。
- 如果 S1/S2 发现 `findHoles()` source history 没有完整消费 profile source，S3 才补 history/topo，而不是在 executor 输出端修剪。

### 明确非目标

- GUI conic edit、full sketch solver conic constraints、DistanceType default/todo。
- GUI Hole dialog、read-only UI 状态、Workbench task panel。
- Full FreeCAD Hole parity、arbitrary external standard tables、persistent backend session。
- Full topo naming / full MapperHistory 迁移；本包只处理 Hole 发布边界需要的 history freeze / element status。

## S0-S5 闭环

### S0 live 基线

冻结 `pwd`、`HEAD`、`git status`、C6 队列状态、`part_design.hole` capability、Hole focused tests 和当前 pending expected rows。S0 不改代码。

### S1 源码与 oracle 矩阵

读取 FreeCAD `src/Mod/PartDesign/App/FeatureHole.cpp` 的属性注册、标准表、thread diameter、execute、findHoles、makeThread 调用链；读取 cad-core `feature_hole.cpp` 和 tests；批量采集或列出需要采集的 FreeCAD oracle。

### S2 路由裁决

逐 row 裁决为：

- `already_closed_expected_backed`
- `publication_closure_only`
- `oracle_pending_collect`
- `backend_gap_requires_implementation`
- `historical_or_native_blocked`
- `non_goal`

S2 是代码闸门；没有 S2 裁决，不得改 C++、fixtures、expected 或 assertions。

### S3 实现或 no-code closure

若 S2 接受 active backend gap，按 FreeCAD 源码链补实现，优先落到 `cad-core/src/part_design/feature_hole.cpp` 和必要的 `part/topo` / `topo` history API，不得从 fixture 输出倒推业务逻辑。若没有 active backend gap，S3 只做 capability/test/docs publication assertion。

### S4 fixtures / tests / capability / docs

已完成 docs-only 发布同步：没有修改 fixture expected、focused tests、adapter capability C++ 或 test assertions，只把既有 FreeCADCmd expected、focused unittest 和 capability assertions 同步到矩阵、README、总入口和步骤状态。旧 pending rows 保留为 `historical_or_native_blocked` / historical non-active legacy，不再挂在 active gap；expected-backed rows 明确依赖 `FreeCADCmd oracle from ...` expected，而不是从当前 `cad-core` 输出倒推。

### S5 release gate

已运行 release gate 并完成发布收口：S5 起点 `HEAD=dd2b919e46`、工作区干净；`cmake --build build` 通过，S4/S5 focused unittest 5 tests OK；步骤文件名已标记为 `【已实现】`，队列为空。S5 没有修改 collector、expected 批量文件、topo/history 主路径或 adapter C ABI schema，因此未触发重型收口。

## 验收命令分层

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n 'part_design.*hole|model_thread|hole_threaded_model_thread|hole-supported-model-thread|hole-supported-threaded' cad-core/src/runtime/capability_contract.cpp cad-core/src/part_design/feature_hole.cpp cad-core/tests docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

### 阶段回归

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_supported_threaded_heads_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_model_thread_builds_freecad_pipe_shell_tool \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_thread_table_model_thread_contract_uses_native_oracles \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_threaded_model_thread_head_cut_oracle_matrix_matches_native \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

### 重型收口触发条件

只有当 S3/S4 修改 collector expected 语义、批量 expected JSON、topo/history 主路径、Body cut replay 或 adapter C ABI schema 时，才追加：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures tests.test_p7_features tests.test_adapters
```
