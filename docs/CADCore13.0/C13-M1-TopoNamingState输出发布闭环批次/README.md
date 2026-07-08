# C13-M1 TopoNamingState 输出发布闭环批次

C13-M1 目标是让 `cad-core recompute` 的正式 response 携带可消费的 `topoNamingState`，并让输出结构对齐 `cad-core/fixtures/<phase>/expected/*.freecad.json` 中的 `topoNamingState` schema。

当前发布状态：C13-M1 已完成。`app::parseDocument()` 会读取请求侧 `topoNamingState`，`ComputeContext` 会保存它，`runtime/recompute.cpp` 会用输入 state 的 `elementMap.entries` 补回 `NamedShape.elementMap`，reference resolution 优先消费 `NamedShape` / `ElementMap`；正式 `recomputeResultJson()` 现在会把本轮 `context.namedShapes`、`responseSubshapes()` 和对象 hash 打包成新的顶层 `topoNamingState`。

发布闸门结论：

- 正式 response 已发布顶层 `topoNamingState`，schema 为 `cad-core.topo-state.v1`。
- response state 可注入下一次请求，`c4m6/topo-state-body-tip-stable-recovery` 的 Body/Tip stable reference 不回退。
- CLI / C API / worker / wasm 共享同一正式 `topoNamingState` channel。
- 5 个 focused `cad-core-res` 输出已保存到 `cad-core/fixtures/<phase>/cad-core-res/<case>.cad-core.json`，未写入 `expected/`。
- full `CadCoreAdapterTest` 仍保留既有 `6 failures, 8 errors` baseline；C13-M1 focused adapter / round-trip / fixture 验证通过，不作为本批次 blocker。
- FreeCAD raw mappedName、childElementMapKey、mapperHistoryIds 字节级 parity 是后续批次，不在 C13-M1 从 expected 字符串反推 runtime 实现。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=d96a4c90de`（`d96a4c90de chore: 清理失败 fixture 并补采集基线`）。
- 创建前 `git status --short` 无输出。
- `docs/CADCore13.0` 创建前为空目录。

## 核心方案

| 项 | 设计 |
| --- | --- |
| Module | `runtime/topo_naming_state`，负责把本轮 request-local `NamedShape` 账本发布成 response topo state。 |
| Seam | `recomputeResultJson()` 已生成每个 target 的 response `subshapes` 后、组装最终 JSON 前。 |
| Interface | `topoNamingStateJson(document, context, responseSubshapesByObject)`，返回一个完整 `cad-core.topo-state.v1` JSON。 |
| Inputs | `app::Document`、`ComputeContext`、每个 target 的 `responseSubshapes()` 输出。 |
| Outputs | 顶层 `topoNamingState`，包含 `schemaVersion`、`producer`、`documentHash`、`objects`。 |
| Consumers | 下次 recompute 的 `document.topoNamingState`、`mergeTopoNamingStateElementMap()`、`resolveElementReference()`、adapter/worker/wasm response。 |

## FreeCAD source authority

| 语义 | FreeCAD source | C13-M1 用法 |
| --- | --- | --- |
| ElementMap 是稳定拓扑身份账本 | `src/App/ElementMap.cpp` | `topoNamingState.objects[*].elementMap.entries` 只能来自 `NamedShape.elementMap` / history evidence，不从 response 字符串猜。 |
| `TopoShape::makeShapeWithElementMap()` 先保留 source subelement，再消费 mapper history | `src/Mod/Part/App/TopoShapeExpansion.cpp` | `NamedShape` / `MapperHistory` 是发布 state 的源，不在 adapter 层补业务规则。 |
| Geo reference 通过 ElementMap 更新 | `src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()` | 输入侧 persisted state 恢复旧 stable token；输出侧必须发布下次请求可消费的同一类证据。 |
| Body display shape 与 Tip child path 需要分离 | `src/Mod/PartDesign/App/Body.cpp::Body::execute()` | response `subname/fullSubname/stableSubname` 与 topo state target 不混用；Body/Tip alias 仍由 `NamedShape` 证明。 |
| native oracle state schema | `cad-core/tools/collect_freecad_expected.py::topo_naming_state_response()` | 只作为 expected schema 和 comparable behavior 依据，不作为 runtime 业务语义来源。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/include/cad_core/runtime/topo_naming_state.h` | 新增小接口，隐藏 state 发布细节。 |
| `cad-core/src/runtime/topo_naming_state.cpp` | 实现 document/object hash、producer 继承、object payload、element map entries、child maps、mapper history 发布。 |
| `cad-core/src/runtime/recompute.cpp` | 缓存 target response subshapes，最终 payload 增加 `topoNamingState`。 |
| `cad-core/src/part/topo_shape.cpp` / `topo_shape_mapper.cpp` | 复用 `NamedShape`、`NamedShapeChildMap`、`mapperHistoryToJson()`，必要时补 request-local state 专用序列化 helper。 |
| `cad-core/tests/test_adapters.py` | FFI / worker / wasm response 必须都带 `topoNamingState`。 |
| `cad-core/tests/test_expected_fixtures.py` 或新增 focused test | 对 p2/p5/c4m6 等小范围 expected topo state 做结构对齐和可消费性断言。 |

## 分批目标

### 第一批：response state 闭环

- 正式 response 增加 `topoNamingState`。
- `schemaVersion=documentHash=objects` 等基本字段稳定输出。
- target objects 的 `subshapes` 从 `responseSubshapes()` 收集，保留 `subname`、`identityStatus`、可选 FreeCAD mapped-name evidence。
- `elementMap.entries` 从 `NamedShape.elementMap` 发布，只写有稳定证据的非 indexed-only 条目。
- `childElementMaps` 和 `mapperHistory` 先用当前 cad-core request-local schema 输出；缺少 FreeCAD 原生 mapped-name 时不伪造。

### 第二批：expected 对齐收窄

- p2 代表性 PartDesign Body fixture 能输出 `topoNamingState` 并被下一次请求消费。
- p5 sketch internal/raw identity fixture 能输出 indexed-only 或 history_partial state。
- c4m6 topo-state recovery fixture 保持输入 state 恢复能力不回退。
- 对于 `Pocket.#...:H...,F` 这类 FreeCAD mapped-name 原文不一致项，记录为 `mapped_name_encoding_gap`，不得用 fixture 特判补。

### 后续批次：原生 mapped-name parity

- 单独实现 FreeCAD mapped-name encoder / canonicalizer。
- 单独补 `childElementMapKey` 和 mapperHistory id 关联。
- 单独扩大到全量 `fixtures/<phase>/expected/*.freecad.json` topo state parity。

## 工作步骤

- 入口：核对包结构、矩阵、步骤队列。
- S0：冻结 live 基线、现有 code path 和 expected schema。
- S1：复核 FreeCAD / collector 输出合同，确认哪些字段是 C13-M1 必须对齐，哪些进入后续 mapped-name parity。
- S2：补 focused red tests：正式 response 必须带 `topoNamingState`，且 persisted state 可被下一次 recompute 消费。
- S3：实现 runtime topo state 发布器，并接入 `recomputeResultJson()`。
- S4：验证 adapters、fixture expected 小范围和 reference update 不回退。（已完成：focused output、adapter channel、round-trip、legacy branch 均通过，剩余 expected 差异已分类。）
- S5：发布闸门已完成；矩阵、README、方案和剩余 mapped-name gap 均已收口。

## 非目标

- 不在 executor、adapter 或 frontend 输出层拼 fixture 专用 `Pocket.#...` mapped names。
- 不实现完整 FreeCAD mapped-name 字节级编码。
- 不把 BREP、TopoDS、NamedShape、ElementMap、mesh 保存为跨请求 backend 状态。
- 不改变 `subname/fullSubname/stableSubname` 的既有 response 语义。
- 不扩大到全量 expected fixture parity，除非 C13-M1 focused 闭环通过后另开批次。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M1-TopoNamingState输出发布闭环批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore13.0
git diff --check
```

实现 focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
build/cad-core recompute fixtures/p2/rect-pad-pocket.json --output out/c13m1-rect-pad-pocket.result.json
jq '.topoNamingState' out/c13m1-rect-pad-pocket.result.json
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel
```

阶段收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 cad-core/tools/collect_freecad_expected.py --phase p2 --check --skip-unsupported
cd cad-core
python3 -m unittest discover tests
```
