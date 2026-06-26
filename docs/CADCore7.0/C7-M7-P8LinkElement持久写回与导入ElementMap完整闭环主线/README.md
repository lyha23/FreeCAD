# C7-M7 P8 LinkElement 持久写回与导入 ElementMap 完整闭环主线

本目录承接 C7-M6 release gate 之后的 P8 引用闭环方向。C7-M6 已确认 Assembly Joint 非 identity marker chain 为 expected-backed closed，zero Angle fallback class evidence 与 bundled `offsetPlc` lifecycle 继续 `oracle_blocked`，没有 `backend_gap_requires_implementation`。C7-M7 不继续扩 Assembly Joint，而是转向 P8 中仍未迁移的 Link / ElementMap / stable reference 生命周期。

C7-M7 的目标是围绕导入 shape 完整 `ElementMap`、`ShowElement=true` 的 `LinkElement` / `LinkGroup` 持久写回事务、cross-document 文档哈希 / postfix 生命周期和复杂多层 `LinkSub` 链，先以 FreeCAD source 和 checked-in native expected 证明候选范围，再决定是否打开 `cad-core` implementation gate。

## 入口

- 主线总入口：`6-26-14-20-C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线总入口.md`
- 方案：`6-26-14-20-C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环方案.md`
- 工作步骤总入口索引：`工作步骤细分/6-26-14-20-【已实现】C7-M7工作步骤总入口.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`），创建前 `git status --short -uall` 无输出。
- C7-M1 到 C7-M6 队列均为空；C7-M6 final route 为 `expected-backed / oracle_blocked / no backendGap`。
- P8 live 文档仍明确未迁移：导入 shape 完整 `ElementMap`、完整 FreeCAD Link 账本、`ShowElement=true` LinkElement / LinkGroup 持久写回事务生命周期、完整 cross-document 文档哈希生命周期和更复杂 LinkSub 链。
- 当前 Link 能力只覆盖请求内 display、拾取、alias retag、terminal / merge history 传播和前端图更新建议；`documentObjectUpdates` 是前端更新 graph 的建议，不是后端持久化状态。
- C7-M7 初始队列为 S0-S6 pending；S0/S1 只允许文档和矩阵，S2 形成 oracle 候选批次，S3 才允许采 native oracle 或 blocker，S4 裁决 implementation gate，S5 只有在 S4 打开 `backend_gap_requires_implementation` 时才改 C++。
- S0 live 基线已冻结：执行时 `pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=7be2d4e937`（`7be2d4e937 docs: 完成 C7-M6 S6 发布闸门`），开始状态只包含 `docs/CADCore7.0/README.md` modified 和本 C7-M7 文档包 untracked 文件；C7-M1 到 C7-M6 队列均为空，C7-M7 初始队列从 S0 开始。
- S0 P8 边界已冻结：already-covered 包括基础 Link / LinkSub / LinkGroup / LinkElement display、ElementList / ElementCount / ShowElement 请求内 `documentObjectUpdates` 建议、FullSubList / mapped postfix / cross-document alias 更新、Link retag terminal / merge history、plain group 展开、BREP / STEP / IGES import `history_partial` ElementMap；remaining 包括完整导入 ElementMap、STL import `indexed_only` 后续、完整 FreeCAD Link 持久账本、ShowElement 持久写回事务、复杂多层 LinkSub 生命周期和 cross-document hash / postfix 生命周期；diagnostic / non-goal 包括 GUI / ViewProvider / Workbench、前端同步协议、跨请求 backend cache / persistent BREP、Worker / WASM / Web 产品化和 Assembly Joint blocked rows。
- S0 已关闭 `C7M7-BLOCKER-000` / `C7M7-GATE-000`，没有采 oracle、没有新增 fixture/expected/test、没有改 C++；下一队列项是 S1 source / coverage 复核。

## 收口边界

- 先证明 FreeCAD source authority 和 native oracle，再比较 current `cad-core`；不得从 current `cad-core` 输出倒推 expected。
- 只处理 P8 Link / imported-shape stable reference 闭环：导入 shape ElementMap、LinkElement / LinkGroup 持久写回、cross-document hash / postfix、复杂 LinkSub 解析和 `elementReferenceUpdates` / `documentObjectUpdates` 语义。
- 不处理 GUI / ViewProvider / Workbench、前端状态同步协议、跨请求后端缓存、BREP 持久状态、Worker / WASM / Web 产品化或 Assembly Joint blocked rows。
- 如果 FreeCAD native lifecycle 不可采、source authority 不足或 current `cad-core` 已匹配，必须发布为 `oracle_blocked`、`diagnostic_non_goal` 或 `already_closed_expected_backed`，不打开 C++ 实现。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线 docs/CADCore7.0/README.md
git diff --check
```
