# 【已实现】C13-M1 TopoNamingState 输出发布闭环批次方案

## 背景

`cad-core/fixtures/<phase>/expected/*.freecad.json` 已经普遍包含 `topoNamingState`。S3/S4 已补齐第一轮输出发布闭环：请求里的 `topoNamingState` 会进入 `Document` / `ComputeContext`，并在 `registerIndexedNamedShape()` 阶段把 persisted `elementMap.entries` 合并到当前 `NamedShape.elementMap`，用于 stable reference 恢复；正式 response 也会发布新的顶层 `topoNamingState`。

C13-M1 S5 已完成发布闸门和 follow-up 拆分；FreeCAD raw mapped-name、child map key、mapper history id 字节级 parity 不在 C13-M1 从 expected 字符串反推实现。

## 发布状态

- 状态：completed / 已完成。
- 正式 response：已发布顶层 `topoNamingState`。
- 可消费性：response state 可注入下一次请求，`c4m6` Body/Tip stable reference 不回退。
- Adapter channel：CLI / C API / worker / wasm 的 `topoNamingState` payload 一致。
- Focused outputs：5 个 `cad-core-res` 输出已保存，且未写入 `expected/`。
- 已知 baseline：full `CadCoreAdapterTest` 仍是既有 `6 failures, 8 errors`，不作为 C13-M1 blocker。
- 后续 gap：`freecad_mapped_name_encoding_gap`、`child_element_map_key_gap`、`mapper_history_id_gap` 进入后续批次。

## 问题定义

当前 response 的 `subshapes[].stableSubname` 可以证明某些当前结果有稳定引用，但它不是完整 state：

1. 它没有顶层 `schemaVersion`、`documentHash`、`producer`。
2. 它没有按 object 汇总 `elementMap.entries`。
3. 它不能直接作为下一次请求的 persisted topo state。
4. 它不能表达 child element map、mapper history、deleted/split/ambiguous 这类非唯一状态。

因此不能靠 adapter 在输出层简单复制 `subshapes[].stableSubname`。必须从 `context.namedShapes` 这个 request-local identity ledger 发布。

## 核心设计

新增 runtime 模块：

```cpp
nlohmann::json topoNamingStateJson(
    const app::Document& document,
    const ComputeContext& context,
    const std::map<std::string, nlohmann::json>& responseSubshapesByObject
);
```

该接口的约束：

- 只读 `document`、`context`、已生成的 response subshapes。
- 不修改 `NamedShape`、`subshapes`、`elementReferenceUpdates`。
- 不依赖 adapter 类型；CLI、C API、worker、wasm 都得到同一 response field。
- 不把 expected fixture 里的 mapped name 字符串作为输入或 fallback。

## 输出 schema

顶层：

```json
{
  "schemaVersion": "cad-core.topo-state.v1",
  "producer": {
    "cadCoreVersion": "cad-core-runtime-v1",
    "freecadVersion": "cad-core-runtime",
    "occtVersion": "<kernelVersion>"
  },
  "documentHash": "sha256:...",
  "objects": {}
}
```

object payload：

```json
{
  "objectHash": "sha256:...",
  "elementMapVersion": "cad-core.element-map.v1",
  "subshapes": {},
  "elementMap": {
    "encoding": "cad-core.element-map.v1",
    "status": "indexed_only",
    "entries": {}
  },
  "childElementMaps": [],
  "mapperHistory": []
}
```

## ElementMap 发布规则

- `entries` 来源是 `NamedShape.elementMap`，不是 fixture expected。
- `stableName == currentName` 的 indexed-only alias 默认不写 entry；这类状态通过 `status=indexed_only` 表达。
- 当 stableName 能映射到 response subshape 时写入 entry：
  - `target.object` 使用当前 target object。
  - `target.subname` 使用 response subshape 的 `subname`。
  - `source.object/subname` 使用 ledger source；如果当前只有 object-local ledger，则保守写当前 object 和 indexed subshape。
  - `shapeKind` 从 response subshape kind 推导。
  - `recoverability` 从 mapper history / terminal history 推导，默认 `resolved`。
  - `mappedName.raw/canonical` 暂用 stable token；FreeCAD 原生 `#...:H...` 编码缺口记入矩阵，不伪造。

## Hash 规则

- C13-M1 可以复刻 collector 的 `semantic_hash()`：`json.dumps(..., sort_keys=True, separators=(",", ":"))` 的 SHA256。
- C++ 实现需对 `Objects` 和 `recompute` 使用稳定 JSON dump；如当前 `app::Document` 不保留 raw `recompute`，S0/S2 应先确认是否需要在 parse 层保留最小 raw document hash 输入。
- 如果第一轮不能做到与 collector 完全相同的 `documentHash/objectHash`，测试必须把 hash 差异归类为 `hash_encoding_gap`，不得因此改 fixture expected。

## 实施边界

第一轮已做到：

- 正式 response 包含 `topoNamingState`。
- target object 的 `subshapes`、`elementMap.status`、`entries` 能表达当前 ledger。
- 输出 state 可被下一次请求读入并参与 reference resolution。
- adapter / worker / wasm 同步输出。

后续再做：

- FreeCAD mapped-name 字节级编码。
- `childElementMapKey` 与 mapper history id 关联。
- 全量 expected topo state parity。

## 最小完整语义批次

| 场景 | 目的 |
| --- | --- |
| `p2/rect-pad-pocket` | PartDesign Body 有 history_partial state，验证 response 包含 object topo state。 |
| `p5/sketch-internal-face` | indexed-only / empty state 不应伪造稳定身份。 |
| `c4m6/topo-state-body-tip-stable-recovery` | persisted state 输入仍能恢复 Body/Tip stable reference。 |
| `p6/up-to-face-stable-body-history` | old StableSubList 通过发布 state 参与下一次 reference resolution。 |
| `p8/app-link-box-face` | Link / child path 不应把 display path 误当 durable identity。 |

## 验收分层

本轮短跑只验证文档和队列：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
git diff --check
```

实现 focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
build/cad-core recompute fixtures/p2/rect-pad-pocket.json --output out/c13m1-rect-pad-pocket.result.json
jq '.topoNamingState.objects.Body.elementMap.status' out/c13m1-rect-pad-pocket.result.json
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel
```

阶段收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover tests
```

## 关闭条件

- C13-M1 focused fixtures 能看到正式 response `topoNamingState`：已完成。
- 输入 state -> recompute -> 输出 state -> 下一次输入 state 的闭环至少有一个 Body stable reference 测试覆盖：已完成。
- `subname/fullSubname/stableSubname` response 既有测试不回退：focused 验证已完成。
- mapped-name 字节级差异被矩阵明确归类为后续批次，而不是混进输出层特判：已完成。
- C13-M1 blocker queue 已全部 closed；S5 后队列无 pending 步骤。
