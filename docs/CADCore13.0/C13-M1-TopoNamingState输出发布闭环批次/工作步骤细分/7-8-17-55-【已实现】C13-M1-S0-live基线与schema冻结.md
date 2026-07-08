# 【已实现】C13-M1 S0 live 基线与 schema 冻结

## 目标

冻结当前代码路径和 expected `topoNamingState` schema，确认 C13-M1 只补输出发布闭环。

## live 基线

- `pwd`: `/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`: `0055b6ccdf`
- `git log -1 --oneline`: `0055b6ccdf 文档：关闭 C13-M1 队列入口`
- `git -c core.quotepath=false status --short -uall`: 无输出，S0 开始前工作区干净。
- `step_goal_queue.py ... --format markdown`: S0 开始前第一项为 `7-8-17-55-C13-M1-S0-live基线与schema冻结.md`。

## 当前 code path 冻结

- 输入解析存在：`cad-core/src/app/document.cpp::parseDocument()` 会在 root JSON 中读取对象型 `topoNamingState`，并保存到 `document.topoNamingState`。
- runtime 保存 input state：`cad-core/include/cad_core/runtime/compute_context.h::ComputeContext` 有 `nlohmann::json topoNamingState` 字段；`cad-core/src/runtime/recompute.cpp::recomputeContext()` 将 `document.topoNamingState` 赋给 `context.topoNamingState`。
- 当前 alias 恢复只消费输入侧 state：`mergeTopoNamingStateElementMap()` 读取 `context.topoNamingState.objects[name].elementMap.entries`，只在 `target.object == name` 且 `target.subname` 存在于当前 `NamedShape.elements` 时写入 `namedShape.elementMap[stableSubname] = currentSubname`；`registerIndexedNamedShape()` 负责先建立或取出当前 `NamedShape` 后再调用该 merge。
- 当前不发布输出 state：`recomputeResultJson()` 只返回 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads`，没有顶层 `topoNamingState`。
- collector schema 仅作为 expected 合同样例：`cad-core/tools/collect_freecad_expected.py::topo_naming_state_response()` 生成 `schemaVersion`、`producer`、`documentHash`、`objects`，object payload 由 `topo_state_object_payload()` 生成。

## expected schema 抽样

抽样命令：

```bash
jq '.topoNamingState | keys' cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json
jq '.topoNamingState | keys' cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json
```

两个样本的顶层 keys 一致：

```json
[
  "documentHash",
  "objects",
  "producer",
  "schemaVersion"
]
```

object payload 抽样：

| fixture | object | object payload keys | elementMap keys |
| --- | --- | --- | --- |
| `cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json` | `Body` | `childElementMaps`, `elementMap`, `elementMapVersion`, `mapperHistory`, `objectHash`, `subshapes` | `encoding`, `entries`, `status` |
| `cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json` | `Sketch` | `childElementMaps`, `elementMap`, `elementMapVersion`, `mapperHistory`, `objectHash`, `subshapes` | `encoding`, `entries`, `status` |

## 非目标冻结

- C13-M1 S0 不实现 S1-S5，不修改 `cad-core/src`、`cad-core/include`、`cad-core/tools`、fixtures 或 tests。
- C13-M1 不做 FreeCAD mapped-name 字节级编码；`Pocket.#...:H...` 一类 raw mapped name parity 进入后续批次。
- C13-M1 不做全量 fixture parity，只做后续步骤定义的 focused 输出发布闭环。
- C13-M1 不改 frontend，也不把 expected fixture 字符串反推为 runtime 实现规则。
- S0 未采集 FreeCADCmd oracle，未运行 build/full unittest。

## 关闭条件

- `矩阵/c13m1_topo_state_scope_matrix.tsv` 中 S0 行更新为 `closed`。
- `矩阵/c13m1_topo_state_contract_matrix.tsv` 明确 C13-M1 必须字段和后续字段。
- blocker queue 中 S0 blocker 关闭。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
jq '.topoNamingState | keys' cad-core/fixtures/p2/expected/rect-pad-pocket.freecad.json
jq '.topoNamingState | keys' cad-core/fixtures/c4m6/topo-state-body-tip-stable-recovery.json
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```
