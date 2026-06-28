# C10-M1-S4 复杂 open-wire 与 WireJoiner 账本专项复审

## 目标

复核复杂开放线网和 WireJoiner history ledger，判断哪些 open-wire / split / branch case 能产生唯一 ElementMap alias，哪些必须保持 stable diagnostic。S4 不靠输出修剪解决 ownership。

## live 基线

| 命令 | S4 记录 |
| --- | --- |
| `pwd` | `/home/user/Chili3DProject/FreeCAD` |
| `git rev-parse --short HEAD` | `a00fa181e7` |
| `git log -1 --oneline` | `a00fa181e7 docs: 完成 C10-M1 S3 FreeCAD oracle 复审` |
| `git -c core.quotepath=false status --short -uall` | 无输出，工作区干净。 |
| C10-M1 queue | 下一项为 S4。 |

## FreeCAD 依据

- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::initWireInfo()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::splitEdges()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::findTightBound()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::exhaustTightBound()`
- `src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`

## 范围

| 账本轴 | 必须观察 | S4 路由 |
| --- | --- | --- |
| branch open cutter | connected branch 是否进入 openWireCompound 或被 bounded face 消费 | S6 implementation 或 diagnostic retained。 |
| multi-result open wires | child-wire ledger 是否能唯一定位 current member | S6 implementation 或 stable diagnostic。 |
| one-source-to-many | source edge split 成多个 InternalEdge 时是否有唯一 selector | diagnostic retained，除非 evidence 唯一。 |
| deleted source | noOriginal filtered raw Edge / Vertex 是否写 terminal deleted history | release gate 或 implementation candidate。 |

## S4 账本复审结论

| 轴 | evidence | S4 裁决 | S6 gate |
| --- | --- | --- | --- |
| branch open cutter | `cad-core/fixtures/p5/sketch-internal-face-branch-open-cutter*` 已有 FreeCAD 20260519 expected，current legacy 抽样为 `InternalFace=2` / `InternalEdge=10` / `InternalVertex=11`，`wire_joiner.open_export_count=2`、`history_event_count=2`、`has_open_wires=true`。publisher 只在 child-wire ownership event 可定位目标且每个 source 只有一个 target 时写 `wire_joiner_history:element_map`。 | unique child-wire evidence 子集已覆盖；没有新的 current mismatch。`open_wire_compound_target_not_found` 或 split one-to-many 仍是 reselect / diagnostic，不提升为任意 alias。 | `no_gap` / no-code release gate；不打开 `wire_joiner.cpp` 实现候选。 |
| multi-result open wires | `t-cutter` current 抽样为 `open_export_count=8`、relations 包含 `generated` 与 `split`，`test_p5_wire_joiner_open_export_uses_publication_events` 要求 mapper event 来源为 `WireJoiner` 且不泄漏 producer anatomy。 | 多 child-wire 可在唯一 child-wire ownership 下产生 mapper event / 单 target ElementMap alias；多 target 或 target 缺失不做输出排序选择。 | `diagnostic_retained` for ambiguous entries；S6 只发布已覆盖行为。 |
| one-source-to-many split | `through-open-cutter` / `touching-open-cutter` / `near-overlap` expected-backed current counts 匹配；tests 明确 `Edge5` 不进入 `internal_element_map` / `named_shape.element_map`，只保留多个 split mapper history。 | one-source-to-many 是 split history + `subname_split_requires_reselect`，不是唯一 stable selector。 | `diagnostic_retained`；不得用 source index、split order、bbox、面积、长度或输出顺序选一个 target。 |
| deleted source / noOriginal purge | `dangling-line`、`split-and-dangling`、`internal-branch-cutter` current 抽样都有 `no_original_purged` / `open_wire_result_empty_after_filter`，history 记录 `Edge5/Vertex6` 或 `Edge6/Vertex7/Vertex8` deleted，tests 要求不写唯一 map。 | terminal deleted history 已具备；deleted raw Edge / Vertex 是 stable diagnostic / reselect evidence，不是 implementation gap。 | `diagnostic_retained` / no-code release gate。 |
| overlap / no open export | `overlap-rectangles`、`overlap-circles`、`overlap-bsplines-empty` 已有 expected；current 无 open export 或 InternalShape empty，未形成 S4 WireJoiner implementation candidate。 | 归入 FaceMaker / count-level no-gap 或 empty InternalShape retained behavior。 | `no_gap`。 |

本轮没有新增 c10m1 native fixture / expected。现有 P5 expected 与 S3 C10M1 expected 已足以给 S4 route 定性；current CLI 抽样只在 expected / source evidence 之后用于确认没有新的 mismatch。

## 已回写的矩阵行

- `C10M1-SCOPE-103`：`no_gap` / S6 no-code gate。
- `C10M1-SCOPE-104`：`diagnostic_retained`。
- `C10M1-BLOCKER-401`：`closed_s4`。
- `C10M1-CAT-102`：`no_gap_diagnostic_retained`。

## S6 gate

- `C10M1-SCOPE-103`：`no_gap` / no-code gate。复杂 open-wire 的 expected-backed counts 和 current WireJoiner ledger diagnostics 已匹配；没有具体 fixture、expected、C++ landing 或 focused test 需要交给 S6 实现。
- `C10M1-SCOPE-104`：`diagnostic_retained`。S6 只能发布 one-source-to-many、vertex multiplicity、summary_only、missing child-wire 或 noOriginal deleted 的 retained diagnostic；不得把它们改成 supported alias。
- reopen condition：只有新增 native oracle 证明 current cad-core 缺少唯一 child-wire / mapper history evidence，且该 evidence 不依赖几何排序或输出端修剪，才可由 S6 打开 `cad-core/src/part/wire_joiner.cpp` / `cad-core/src/part/internal_shape_history_publisher.cpp` implementation candidate。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'WireJoiner|openWireCompound|EdgeInfo|WireInfo|aHistory|multiplicity|terminal deleted|InternalEdge|InternalVertex|summary_only|child-wire|open_export' cad-core/src/part cad-core/src/app cad-core/tests cad-core/fixtures/p5 cad-core/fixtures/c10m1 docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0 cad-core/fixtures/c10m1
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M1-SketchOpenWireInternalFaceStableSelector批次/工作步骤细分 --format markdown
```

## 验收标准

- 每个复杂 open-wire route 都说明是否具备唯一 ElementMap / mapper history evidence。
- 一对多或多解 case 必须保持 stable diagnostic，不得选择任意 target。
- 若 S4 打开 S6 implementation gate，必须列出具体 fixture、expected、C++ 落点和 focused test。

## 验收记录

- `CAD_CORE_TEST_LEGACY_OUTPUT=1 cad-core/build/cad-core recompute ...` 抽样覆盖 branch-open-cutter、t-cutter、through-open-cutter、dangling-line、split-and-dangling、internal-branch-cutter、overlap-rectangles、overlap-circles、overlap-bsplines-empty、C10M1 touching open cutter 和 near-overlap rectangles：CLI 均成功。
- 抽样结论：branch/t/through/touching/near-overlap 的 open export 均有 child-wire / split history evidence，未发现 expected-backed current mismatch；dangling/split-and-dangling/internal-branch 的 noOriginal purge 输出 terminal deleted / diagnostic，不写唯一 map；overlap closed profiles 与 empty BSpline 不是 S4 implementation gap。

## 非目标

- 不新增 source index / split order / geometry sorting fallback。
- 不在 adapter 或 output JSON 末端补业务逻辑。
- 不实现 WireJoiner / ElementMap / adapter C++，不修改 `profile_resolver.cpp`。
