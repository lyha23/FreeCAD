# C13-M4 FreeCADExpectedLedger TopoState 投影闭环批次

C13-M4 承接 `docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 的实现落地：`.freecad.json` 保持 public `topoNamingState` 投影，`.freecad.ledger.json` 承担 FreeCADCmd 权威账本证明，`cad-core` runtime 输出必须对齐 `fixtures/<phase>/expected/*.freecad.json`。

本批次不再扩展对外 expected schema，也不把 ledger 厚块塞回 `.freecad.json`。当前最小完整语义批次是 `cad-core/fixtures/c4m6`，因为它已经同时覆盖 first recompute、Body/Tip recovery、compound child maps、mapper history、ReferenceShadow、schema/producer/hash mismatch 和 sidecar ledger 闭包。

## 当前基线

- ledger 闭包已通过：`python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict` 可验证 9 个 `.freecad.json` 与同名 `.freecad.ledger.json`。
- S2 focused runtime parity 已通过：`python3 -m unittest tests.test_topo_naming_state_response` 完整运行 14 个测试并普通通过。
- S1 已补 runtime projection：actual response 现在发布 `topoNamingState.objects.Compound.subshapes.Child0.Face1`、`Compound:ChildBoxA` projection child map，以及顶层 owner-qualified entry `Compound/ChildBoxA.#f:1;BOX,F`。
- 当前 runtime 保留普通 compound child maps，例如 `Compound:ChildBoxA:Child0` 与 `Compound:ChildBoxB:Child1`；S2 已完成 formal focused-parity closeout，S3 仍保留为包索引收口。
- S0 已冻结：`HEAD=718267783c`，c4m6 strict ledger validator 9/9 通过；focused response test 失败 2 个断言但同一首因均为 `Compound.subshapes.Child0.Face1` missing。actual/expected 最小 diff 是缺 `Compound:ChildBoxA` projection child map、`Child0.Face1` subshape、`Compound/ChildBoxA.#f:1;BOX,F` 顶层 entry；普通 `Compound:ChildBoxA:Child0` / `Compound:ChildBoxB:Child1` child maps 已存在。

## 批次目标

1. 保持 `*.freecad.json` / `*.freecad.ledger.json` 分层不变，先证明 artifact 闭包，再证明 runtime 对齐 public projection。
2. 在 `runtime/topo_naming_state.cpp` 发布 ledger 可解释的 child path subshape，例如 `Child0.Face1`。
3. 发布 reference projection child map，例如 `Compound:ChildBoxA`，并把其 canonical entry 合并到顶层 `elementMap.entries`。
4. 让 `c4m6` focused topoNamingState response tests 普通通过。
5. 只在 `c4m6` 绿后，再决定是否把同一模式扩展到其它 phase。

## FreeCAD source authority

| 语义 | FreeCAD source | C13-M4 用法 |
| --- | --- | --- |
| child ElementMap 作为嵌套账本 | `src/App/ElementMap.cpp::addChildElements()` | `childElementMaps` 不是 response 装饰字段，而是 child-local identity ledger 投影。 |
| compound child path / postfix | `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::createChildMap()` | `Child0.Face1` 这类路径必须从 child map / input reference 投影生成，不从 expected 字符串复制。 |
| reference update round-trip | `src/App/PropertyLinks.cpp` | `StableSubList` 只消费 public topoNamingState 投影，不消费 `.freecad.ledger.json` 厚账本。 |
| mapped-name key canonicalization | `src/App/ElementMap.cpp::hashElementName()` / `dehashElementName()` | canonical key 冲突留在 mapperHistory / ambiguity 证据中，不允许静默覆盖。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/src/runtime/topo_naming_state.cpp` | 发布 `topoNamingState.objects[*].subshapes`、`elementMap.entries`、`childElementMaps` 的 public projection。 |
| `cad-core/src/runtime/recompute.cpp` | 保持 request-level failure 与 response assembly 边界，不承载 FreeCAD 业务规则。 |
| `cad-core/tests/test_topo_naming_state_response.py` | focused parity gate；不要放宽 `c4m6` expected diff。 |
| `cad-core/tools/validate_freecad_expected_ledger.py` | artifact gate；只验证 checked-in expected 与 ledger sidecar 闭包，不调用 runtime。 |

## 非目标

- 不手改 `cad-core/fixtures/*/expected/*.freecad.json`。
- 不把 `oracleMetadata`、`topologyInventory`、`referenceLedger` 等厚块塞进 `.freecad.json`。
- 不读取 `.freecad.ledger.json` 作为 `cad-core` runtime 输入。
- 不复制 expected JSON 字符串到 C++。
- 不用 fixture 名称、phase 名称或 object 名称做特殊分支。
- 不在本批次解决 C13-M3 仍 open 的 p2/p6 raw-key producer ledger blocker，除非它直接阻断 `c4m6` child path projection。
- 不改前端 consumer。

## 工作步骤

- 入口：`工作步骤细分/7-9-18-41-【已实现】C13-M4工作步骤总入口.md`
- S0：已冻结 live baseline、ledger gate 与首个 runtime diff。
- S1：已实现 `Compound` child path projection 发布。
- S2：已实现，`c4m6` focused response parity 转绿；hard fail fixtures 不发布 `topoNamingState`，ReferenceShadow.brep 保持 recovery evidence 边界。
- S3：收口文档、矩阵和 C13.0 索引，确认是否回流 C13-M2/C13-M3。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/矩阵/*.tsv
cd cad-core
python3 tools/validate_freecad_expected_ledger.py --phase c4m6 --strict
python3 -m unittest tests.test_topo_naming_state_response
cd ..
git diff --check
```
