# C13-M5 FreeCADExpected 发布对齐批次

C13-M5 的目标是把 `cad-core` 的正式发布输出对齐到所有 checked-in `fixtures/<phase>/expected/*.freecad.json`。这里的对齐对象只包含 native FreeCADCmd oracle 产生的 `.freecad.json`，不包含协议手写合同 `*.expeted.json`，也不把 `.freecad.ledger.json` sidecar 当作 runtime 输入。

本批次承接 C13-M4：C13-M4 已关闭 `c4m6` child path public projection，但那只是 topoNamingState 最小闭环。C13-M5 要把同一原则推广成 release gate：每个 phase 先用统一比较器生成 cad-core 当前输出、规范化非语义漂移，再把差异归类为 runtime publication、feature geometry、diagnostics、oracle/collector 或环境兼容性问题。

## 当前结论

- S0 live inventory 已冻结：当前仓库有 42 个 phase、475 个 checked-in `expected/*.freecad.json`。
- 所有 expected 都有同名 input `fixtures/<phase>/<case>.json`，也都有同名当前输出 `fixtures/<phase>/cad-core-res/<case>.cad-core.json`。
- `expected/*.freecad.json` 是 native FreeCADCmd oracle；`cad-core-res/*.cad-core.json` 是 cad-core 当前实现输出，不能混放。
- `cad-core-res` 额外文件只记录 extra count，不参与 expected discovery，也不自动成为 release parity 缺口。
- 不能手改 expected 来追 cad-core；cad-core 输出应通过实现、发布策略或明确 known-gap 机制对齐 expected。
- FreeCAD raw mapped-name 中的 `:H...` token 允许随机漂移，严格比较必须先 canonicalize；对象集合、subshape 数量、diagnostic code、stableSubname、elementMap key 不能因 hash 漂移被放宽。
- 第一批 strict lane 仍选 `c4m6`，因为它覆盖 first recompute、Body/Tip recovery、compound child maps、mapperHistory、ReferenceShadow、schema/producer/hash mismatch。
- 本轮 S0 不纳入无关 dirty 文件：`DESIGN.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`。

## S0 比较边界

Discovery 只从 live 命令取得：

```bash
find cad-core/fixtures -path '*/expected/*.freecad.json' -type f | sort
```

固定排除：

- `*.expeted.json`：协议手写合同，不是 native expected。
- `*.freecad.ledger.json`：FreeCADCmd 账本 sidecar，只是 expected provenance/evidence。
- `cad-core-res/*.cad-core.json` extra：当前 cad-core 输出目录里的额外文件，不反向扩大 expected discovery。

字段策略：

- strict：public object set、diagnostic code、results key、subshape path/type/count、stableSubname、mappedName.canonical、canonical elementMap key、childElementMaps key、mapperHistory public event identity、ReferenceShadow 边界。
- canonicalized：`mappedName.raw` 等 raw FreeCAD token 中的随机 `:H...` 片段，只在 comparator 内规整，不改 expected 和 runtime。
- tolerant：明确为数值输出的 bbox、placement、matrix、volume、area、length 等浮点字段，只允许小容差，不允许掩盖拓扑数量或诊断差异。
- ignored-with-evidence：只存在于 `.freecad.ledger.json` sidecar、collector coverage/projection/roundTrip/inputReferences 等 provenance 字段，或 cad-core response 中非 native public expected 的 transport/adapter metadata；忽略时必须能指向证据来源。

## 批次目标

1. 建立 `fixtures/<phase>/expected/*.freecad.json` 到 `fixtures/<phase>/cad-core-res/*.cad-core.json` 的统一发现、生成和比较入口。
2. 明确 strict public expected 比较规则：哪些字段 canonicalize，哪些字段必须严格一致，哪些差异只能归入 known gap。
3. 先关闭 `c4m6` strict public parity，再按 phase 家族扩展到 Part primitives、Sketch/InternalShape、PartDesign Body/DressUp/Pattern、Assembly/App::Link。
4. 把差异归类落到 `runtime/topo_naming_state.cpp`、`runtime/recompute.cpp`、对应 feature executor、geometry/topo helper 或 collector/expected 证据，而不是在 adapter 或测试里修剪输出。
5. 形成 release gate：每个 phase 只有在 expected、cad-core-res、比较报告和 focused tests 都闭合后才能标记 green。

## 入口文件

- 方案：`7-10-00-15-C13-M5-FreeCADExpected发布对齐批次方案.md`
- 总入口：`7-10-00-15-C13-M5-FreeCADExpected发布对齐批次总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 工作步骤

| 步骤 | 主题 | 关闭条件 |
| --- | --- | --- |
| S0 | expected inventory 与比较边界冻结 | 已实现：phase inventory、字段策略、非目标和首批 lane 已冻结。 |
| S1 | strict comparator 与 cad-core-res 生成入口 | 可以按 phase 生成 cad-core-res，并输出 canonical diff / gap report。 |
| S2 | c4m6 strict public parity 红灯基线 | 当前 c4m6 strict diff 被机器化记录，区分发布缺口和协议决策。 |
| S3 | topoNamingState 发布策略对齐 | object set、mapperHistory、hash mismatch、link diagnostic 等 public publication gap 有实现计划和 focused tests。 |
| S4 | phase family 扩展 | 按 fixture 家族推进，不把全量 phase 混成一个不可关闭的任务。 |
| S5 | release gate 收口 | 每个 green phase 都有 expected/cad-core-res/report/test 证据，known gap 可追踪。 |

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check -- docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 docs/CADCore13.0/README.md
```
