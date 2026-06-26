# C7-M7 P8 LinkElement 持久写回与导入 ElementMap 完整闭环主线总入口

## 结论

C7-M7 是 C7-M6 之后的 P8 stable reference follow-up。总览后续队列中，Assembly Joint 已经被 C7-M6 裁为 expected-backed / oracle-blocked 且没有 backend implementation gap；下一类更值得推进的后端语义是 Link / imported-shape 的稳定引用闭环。

当前 gate 已关闭：P8 已覆盖基础 Link / LinkSub / LinkGroup / LinkElement display、ElementList / ElementCount / ShowElement 请求内生命周期建议、hidden / XLink / FullSubList 解析、mapped alias、Link retag terminal 与 merge history 传播、plain group 展开以及导入 shape indexed `NamedShape`。C7-M7 不能凭“完整 Link 账本”直接改 C++。S3 未产生 source-backed native expected；S4 已裁决 ORACLE-202 / 302 / 402 保持 `oracle_blocked`，ORACLE-203 保持 STL `oracle_blocker`，没有 `backend_gap_requires_implementation`。S5 已执行 no-code publication closure，未做 C++、fixture、expected、test、collector、capability 或生成输出改动；S6 release gate 已关闭，队列为空。

## 上游状态

- C7-M6 release gate 已完成，最终 route=`expected-backed / oracle_blocked / no backendGap`。
- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md` 的后续队列当前仍列出 P8：Worker / WASM / Web adapter、导入 shape 完整 ElementMap、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希 / postfix 生命周期、更复杂多层 LinkSub 链，以及 Part surface family。
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md` 已声明当前 Link 能力只覆盖请求内 display、拾取、alias retag、history 传播和前端图更新建议；完整 Link 账本和导入 shape 完整 ElementMap 仍未迁移。
- S0 live 基线已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`），开始状态只包含 `docs/CADCore7.0/README.md` modified 和本 C7-M7 文档包 untracked 文件；C7-M1..C7-M6 队列均为空，C7-M7 从 S0 起步。
- S1 已完成 FreeCAD source 与 current cad-core coverage 复核：source authority 覆盖 `LinkBaseExtension::update()`、`DocumentObject::getSubObject()`、`PropertyXLink*`、Part import、`PropertyPartShape`、`TopoShapeMapper`；current coverage 覆盖 `cad-core/src/app`、`cad-core/src/part`、`cad-core/src/runtime`、`cad-core/src/mesh`、P8 tests 和 fixtures。S1 未采 oracle、未新增 fixture/expected/test、未改 C++；`C7M7-BLOCKER-101` / `C7M7-GATE-101` 已关闭。
- S2 已完成 oracle 候选矩阵：`already_covered` 包括 BREP / STEP / IGES `history_partial` import ElementMap、imported Link chain、现有 Link / LinkSub / LinkGroup / LinkElement display/alias、ShowElement request-local `documentObjectUpdates` 和 cross-document request-graph diagnostics；`oracle_candidate` 只剩完整 BREP / STEP / IGES import ElementMap、ShowElement persistent writeback transaction、复杂多层 LinkSub / cross-document hash-postfix save/restore lifecycle；STL complete Part ElementMap 为 `oracle_blocker`，GUI / frontend / Worker / cross-request backend state 为 `diagnostic_non_goal`。S2 未采 oracle、未改 fixture/expected/test/C++，也未打开 S5 gate。
- S3 已完成 native oracle 采集：FreeCADCmd collector/probe 均未暴露可固化的完整 native lifecycle。ORACLE-202 import payload 缺完整 `ElementMap` / reference-update evidence；ORACLE-302 ShowElement payload 缺持久 writeback transaction 字段且部分 fixture 因 native `ElementList` 只读失败；ORACLE-402 collector/save-restore probe 缺 file/stamp/hash、DocMap、restored `FullSubList`、ReferenceShadow 和 mapped postfix lifecycle，复杂多层 label fixture 在 native FreeCAD 中 Link broken；ORACLE-203 保持 STL mesh-specific `oracle_blocker`。S3 未新增或修改 fixture/expected/test，未改 C++。
- S4 已完成 cad-core parity / implementation gate 裁决：没有 source-backed native oracle 可比较，因此 ORACLE-202 / 302 / 402 route=`oracle_blocked`，ORACLE-203 继续 `oracle_blocker`；implementation gate closed，S5 只允许 no-code 发布，不允许修改 runtime C++、fixtures、expected、tests、collector 或 adapter。
- S5 已完成 no-code 发布收口：执行时 `HEAD=5080d31d76`（`5080d31d76 文档：完成 C7-M7 S4 准入裁决`），开始状态干净。already-covered rows 继续关闭；ORACLE-202 / 302 / 402 发布为 `oracle_blocked`；ORACLE-203 发布为 STL `oracle_blocker`；GUI / frontend / cache / Worker 继续 `diagnostic_non_goal`。`C7M7-BLOCKER-501` 已关闭，`C7M7-GATE-601` 发布为 no-code closure，S6 只做 release gate。
- S6 已完成 release gate：执行时 `HEAD=fb133d0fe6`（`fb133d0fe6 文档：完成 C7-M7 S5 no-code 发布收口`），开始状态干净。release route 为 `C7M7-ORACLE-202/302/402=oracle_blocked`、`C7M7-ORACLE-203=oracle_blocker`；already-covered rows closed，GUI / frontend / cache / Worker 为 `diagnostic_non_goal`，没有 `backend_gap_requires_implementation`。`C7M7-BLOCKER-601` / `C7M7-GATE-701` 已关闭，C7-M7 队列为空；S6 未改 C++、fixture、expected、test、collector、capability 或生成输出。

## 初始范围

- 导入 shape：BREP / STEP / IGES / STL import shape 从 indexed `NamedShape` 走向 source-backed `ElementMap` / stable subname / reference update evidence。
- `ShowElement=true`：LinkElement / LinkGroup 自动创建、认领、同步、删除建议从请求内建议扩展到 FreeCAD source-backed 持久写回事务候选。
- LinkSub：对象名 / `$Label` / numeric index / mapped postfix / `FullSubList` / cross-document hash 的复杂多层解析链。
- `elementReferenceUpdates` / `documentObjectUpdates`：只作为前端 graph 更新建议的无状态边界、字段语义和删除 / reopen 条件。
- capability publication：只在 source-backed expected 与 focused tests 证明后发布 supported。

## S0 冻结边界

- already-covered：基础 Link / LinkSub / LinkGroup / LinkElement display，Link placement / scale / visibility，ElementList / ElementCount，ShowElement 请求内 create / claim / sync / delete 建议，FullSubList / mapped postfix / cross-document alias 更新，Link retag terminal / merge history，plain group 展开，以及 BREP / STEP / IGES import 的 `history_partial` ElementMap expected。
- remaining：完整导入 shape ElementMap，STL import 的 `indexed_only` 后续，完整 FreeCAD Link 账本，ShowElement LinkElement / LinkGroup 持久写回事务，复杂多层 LinkSub lifecycle，完整 cross-document hash / postfix 生命周期。
- diagnostic / non-goal：GUI / ViewProvider / Workbench、前端状态同步协议、跨请求 backend cache / persistent BREP、Worker / WASM / Web 产品化、Assembly Joint zero Angle / `offsetPlc` blocked rows，以及用 current `cad-core` 输出倒推 FreeCAD expected。

## 排除项

- GUI、ViewProvider、TaskPanel、Workbench 命令、前端状态同步协议。
- Worker / WASM / Web service bridge 产品化。
- 后端跨请求缓存、BREP 持久状态或隐式写回请求 graph。
- Assembly Joint zero Angle fallback / bundled `offsetPlc` blocked rows。
- 从 current `cad-core` 输出刷新 FreeCAD expected。

## 步骤队列

1. S0（已实现）：冻结 live baseline、C7-M1..M6 队列和 P8 Link / import 已覆盖边界。
2. S1（已实现）：复核 FreeCAD Link / import / PropertyXLink / topo source 与 current `cad-core` coverage，并把 S2 oracle candidate 输入池矩阵化。
3. S2（已实现）：形成 LinkElement writeback / imported-shape ElementMap / complex LinkSub native oracle 候选矩阵。
4. S3（已实现）：采集 native oracle 或记录 native blocker / diagnostic non-goal。
5. S4（已实现）：用 current `cad-core` 做 parity 和 implementation gate 裁决；无 native expected 可比较，implementation gate closed。
6. S5（已实现）：no-code 发布收口；未改 runtime C++ / fixtures / expected / tests。
7. S6（已实现）：release gate，验证 README / 矩阵 / 队列发布口径并清空队列；关闭 `C7M7-BLOCKER-601` / `C7M7-GATE-701`。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```
