# C13-M2 FreeCAD MappedName Parity 实现批次

C13-M1 已经把 `topoNamingState` 发布链路打通：正式 response 会输出 state，下一次请求能消费该 state，CLI / C API / worker / wasm channel 也已验证一致。C13-M2 的目标不是再做输出字段存在性，而是把 C13-M1 留下的 FreeCAD 字节级 identity evidence 缺口收窄到 focused parity。

## 当前队列状态

- 工作步骤总入口已关闭：`工作步骤细分/7-8-20-16-【已实现】C13-M2工作步骤总入口.md` 已确认包结构、入口 + S0-S6 队列顺序和 8 个 TSV 字段数。
- 入口关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=e3b3379102`（`e3b3379102 文档：新增 C13-M2 mapped name parity 方案`），起点 worktree clean。
- S0 已关闭：`工作步骤细分/7-8-20-17-【已实现】C13-M2-S0-live基线与C13-M1继承冻结.md` 已冻结 live baseline、C13-M1 继承能力、focused fixture 当前差异和继承非目标。
- S0 关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=907a9264d7`（`907a9264d7 docs: 关闭 C13-M2 工作步骤总入口`），起点 worktree clean，C13-M1 队列为空。
- S1 已关闭：`工作步骤细分/7-8-20-18-【已实现】C13-M2-S1-FreeCAD-MappedName源码调用链复核.md` 已冻结 FreeCAD `MappedName` / `ElementMap` / child map / mapper relation source authority。
- S1 关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=10dd70aba8`（`10dd70aba8 docs: 关闭 C13-M2 S0 基线冻结`），起点 worktree clean。
- S2 已关闭：`工作步骤细分/7-8-20-19-【已实现】C13-M2-S2-collector-comparator与expected证据矩阵.md` 已把 collector comparator 与 focused expected evidence 分类为 schema/comparator 合同。
- S2 关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5a12e7fdc6`（`5a12e7fdc6 docs: 关闭 C13-M2 S1 源码权威冻结`），起点 worktree clean。
- S3 已关闭：`工作步骤细分/7-8-20-20-【已实现】C13-M2-S3-focused-red-tests.md` 已新增 guarded focused red tests，锁定 p2/c4m6/p6 FreeCAD `mappedName.raw/canonical` parity、p5/p8 indexed-only no-fake-raw 边界，以及 S5 `childElementMapKey` / `mapperHistoryIds` 空证据守卫。
- S3 关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=696a4d0f29`（`696a4d0f29 文档：关闭 C13-M2 S2 证据矩阵`），起点 worktree clean。
- C13-M3 S5 已回流：C13-M3 S1-S4 解除 S4 的 producer-ledger 前置阻塞。
- S4 已关闭：`工作步骤细分/7-8-20-21-【已实现】C13-M2-S4-mappedName-codec实现.md` 已复核 live codec/runtime/tests，确认 focused `mappedName.raw/canonical` parity 普通通过，runtime 只消费 source-backed producer ledger / codec 输出，并保持 indexed-only no-fake-raw 边界。
- S4 关闭执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=38be8d62a5`（`38be8d62a5 docs: 关闭 C13-M3 S5 发布闸门`），起点存在无关脏改 `DESIGN.md`、`docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md`、`docs/CADCore13.0/README.md` 与未跟踪 C13-M5 包；本步只收口 C13-M2 S4。
- 本批次后续仍从 S5 `childKey 与 mapperId 对齐验证`、S6 `发布闸门` 继续。
- S3 未改 C++ runtime、fixtures、expected、collector、adapter 或 `cad-core-res`，也未把 S5 key/id 空证据标成 implemented/supported。C13-M3 回流同样不把 `childElementMapKey` / `mapperHistoryIds` 标成 supported。

## 当前回流状态

C13-M3 S4 live evidence 显示 producer-backed `mappedName.raw/canonical` 发布路径已可用：`tests.test_topo_naming_state_response` 为 `Ran 15 OK` 且无 expectedFailure，adapter channel 单测为 `Ran 1 OK`。C13-M2 S4 已在当前批次中正式复核并关闭：`cmake --build build`、`tests.test_topo_naming_state_response`、adapter channel 单测均通过。

S5/S6 仍需按 C13-M2 队列处理的事项：

- `child_element_map_key_gap`：expected evidence 里有 `childElementMapKey`，runtime 目前只发布当前 child map 投影。
- `mapper_history_id_gap`：expected 使用 `mapperHistoryIds`，runtime 目前发布 request-local `mapperHistory` 和 `mapperHistoryIndexes`。

C13-M2 只做这三个 evidence 的 focused parity；不做全量 expected fixture parity，不改前端，不把 expected 字符串硬塞进 runtime。

## S0 focused baseline

| fixture | 当前状态 | gap 分类 |
| --- | --- | --- |
| `p2/rect-pad-pocket` | C13-M2 S4 已正式复核 `Body` raw/canonical parity 普通通过。 | `raw_canonical_passed_s4`、`child_element_map_key_gap`、`mapper_history_id_gap` |
| `c4m6/topo-state-body-tip-stable-recovery` | C13-M2 S4 已正式复核 `Body` raw/canonical parity 普通通过，并保持 recovery 基线。 | `raw_canonical_passed_s4`、`child_element_map_key_gap`、`mapper_history_id_gap` |
| `p5/sketch-internal-face` | `Sketch` indexed-only / no-fake-raw 边界在 C13-M2 S4 普通通过。 | `indexed_only_boundary_passed_s4` |
| `p6/up-to-face-stable-body-history` | 旧 focused fixture 已由 `718267783c` 删除，不是 live parity 证据；若恢复 `ProbePad`，必须重新采集 native expected。 | `retired_no_live_fixture` |
| `p8/app-link-box` | 当前 live p8 fixture 的 `BoxLink` indexed-only / no-fake-raw 边界在 C13-M2 S4 普通通过；旧 `app-link-box-face` 已由 `718267783c` 删除。 | `child_path_identity_boundary_passed_s4` |

## FreeCAD source authority

| 语义 | FreeCAD source | C13-M2 用法 |
| --- | --- | --- |
| raw MappedName byte structure | `src/App/MappedName.cpp`, `src/App/MappedName.h`, `src/App/ElementNamingUtils.h` | 作为 raw/canonical mappedName encoder 的字节语义依据。 |
| IndexedName <-> MappedName ledger | `src/App/ElementMap.cpp`, `src/App/ElementMap.h` | `elementMap.entries` 的 key、`mappedName.raw/canonical`、child map key 必须来自 ledger 规则。 |
| encodeElementName / hashElementName | `src/App/ElementMap.cpp::encodeElementName()`, `hashElementName()` | 复刻 `#...:H...`、delete/hash canonicalization 的 focused 行为。 |
| child maps | `ElementMap::hashChildMaps()`, `addChildElements()`, `getChildElements()` | 生成或对齐 `childElementMapKey` evidence，不从 fixture 字符串反推。 |
| shape history propagation | `src/Mod/Part/App/TopoShapeExpansion.cpp`, `TopoShapeMapper.cpp` | mapper history id 和 mapped name 来源的调用链依据。 |
| expected schema/comparator | `cad-core/tools/collect_freecad_expected.py` | 只作为 schema、canonicalization 和 diff comparator 依据。 |

S1 已冻结的关键源码结论：

- `MappedName` 的 raw evidence 是 `data + postfix` 双段合并视图；`fromRawData()` 共享原始字节，`findTagInElementName()` 解析 `;:H<tag>:<len>,<type>`。
- `ElementNamingUtils.h` 中 `;:R`、`;:H`、`;:G`、`;:M`、`;:MG`、`;:U`、`;:L`、`;:C` 是 C13-M2 codec/child key 的字节常量 authority。
- `ElementMap::encodeElementName()`、`hashElementName()`、`dehashElementName()`、`hashChildMaps()`、`addChildElements()`、`getElementHistory()` 是 raw/canonical/child-key/history 的 source authority；后续实现不能从 expected 字符串反填。
- `TopoShapeExpansion.cpp` 通过 `ensureElementMap()->encodeElementName()` 传播同拓扑、generated/modified、upper/lower、combo 和 child map 名称；`TopoShapeMapper.cpp` 提供 generated/modified relation 的基础 source。

## S2 expected/comparator 合同

- `cad-core/tools/collect_freecad_expected.py::canonical_freecad_mapped_name()` 只服务 expected/comparator：将 FreeCAD raw mapped name 中的 `:H...` 和 `;D...` 归一化，降低版本/运行时局部 tag 差异。
- `topo_state_element_map_entry()` 从 `stableSubname` 或 `rawFreecadMappedName` 构造 `mappedName.raw/canonical` 和固定 evidence schema；当前 schema 包含 `childElementMapKey` 与 `mapperHistoryIds`，但 focused expected 里没有非空 key/id 证据。
- `comparable_topo_naming_state()` 对 mapped names、entry keys 和 producer 的 FreeCAD/OCCT 版本做比较归一化；这是 expected 文件比较合同，不是 cad-core runtime source。
- focused expected S2 历史证据：`p2 Body=50 entries`、`c4m6 Body=26 entries`、`p6 ProbePad=26 entries` 有 raw/canonical examples；`p5 Sketch=indexed_only/0`、`p8 BoxLink=indexed_only/0` 是 no-fake-raw 边界。S5 回流后的 live 证据不能继续引用已删除的旧 p6 / `app-link-box-face` fixture；当前 live p8 为 `app-link-box`。
- `childElementMapKey` / `mapperHistoryIds` 当前只作为 schema/future S5 关注，S3 只用测试断言 focused expected 没有非空 key/id evidence，不能因字段存在或空值被标成 implemented。

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/include/cad_core/topo/` 或 `cad-core/include/cad_core/part/` 下的 mapped-name codec helper | 承接 FreeCAD raw/canonical mapped-name 编码与 child key 规则。 |
| `cad-core/src/topo/` 或 `cad-core/src/part/` 对应实现 | 复刻 FreeCAD focused encoder/canonicalizer，保留 FreeCAD source 注释。 |
| `cad-core/src/runtime/topo_naming_state.cpp` | 消费 codec 输出，填充 `mappedName.raw/canonical`、`childElementMapKey`、`mapperHistoryIds`。 |
| `cad-core/tests/test_topo_naming_state_response.py` 或新增 focused test | 锁定 focused fixtures 的 C13-M2 parity。 |
| `cad-core/fixtures/<phase>/cad-core-res/*.cad-core.json` | 保存当前实现输出对照，仍不得写入 `expected/`。 |

## 最小完整语义批次

| fixture | 覆盖目的 |
| --- | --- |
| `p2/rect-pad-pocket` | PartDesign Body / Pad / Pocket 的 mappedName raw/canonical 对齐；C13-M3 已解除 producer-ledger 前置阻塞。 |
| `c4m6/topo-state-body-tip-stable-recovery` | persisted state round-trip 下 Body/Tip alias 不回退；C13-M3 已解除 producer-ledger 前置阻塞。 |
| `p5/sketch-internal-face` | Sketch InternalShape 与 indexed-only 状态不伪造 FreeCAD raw name。 |
| `p6/up-to-face-stable-body-history` | 旧 fixture 已删除，当前不作为 live parity 证据；后续若恢复需重新采集 native expected。 |
| `p8/app-link-box` | Link / child path 不误当 durable identity，child map key gap 单独归类；旧 `app-link-box-face` 已删除。 |

## 非目标

- 不创建前端 my-chili3d consumer 同步任务。
- 不把 `fixtures/<phase>/expected/*.freecad.json` 的 raw mappedName 字符串复制进 runtime。
- 不修改 `subname/fullSubname/stableSubname` 既有 response 语义。
- 不扩大到全量 expected fixture parity；C13-M2 只关闭 focused mapped-name / child-key / mapper-id evidence。
- 不改变 CAD Core 无状态边界，不保存 backend session topology cache。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M2-FreeCADMappedNameParity实现批次/矩阵/*.tsv
git diff --check
```

实现 focused：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel
```

阶段收口候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
for spec in p2:rect-pad-pocket c4m6:topo-state-body-tip-stable-recovery p5:sketch-internal-face p8:app-link-box; do
  phase=${spec%%:*}; case=${spec#*:}
  build/cad-core recompute fixtures/$phase/$case.json --output fixtures/$phase/cad-core-res/$case.cad-core.json
done
```
