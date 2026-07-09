# C13-M3 MappedName Producer Ledger 前置实现批次总入口

## 目标

补齐 C13-M2 S4 暴露的 producer-side ledger 缺口：在 `NamedShape` 生产阶段携带 FreeCAD-equivalent tag / masterTag / op / raw mapped-name provenance，使 runtime 能发布 source-backed `mappedName.raw/canonical`，而不是把 stable token 当成 FreeCAD raw name。本批次已完成，C13-M2 S4 可恢复执行。

## 入口文件

- README：`README.md`
- 方案：`7-8-20-58-【已实现】C13-M3-MappedNameProducerLedger前置实现批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前结论

- C13-M2 S4 的前置 producer evidence blocker 已由 C13-M3 S1-S4 解除；C13-M2 S4 本身仍由 C13-M2 队列继续执行。
- C13-M3 是前置账本批次，不关闭 C13-M2 S5/S6，也不把 `childElementMapKey` / `mapperHistoryIds` 标成支持。
- S5 已关闭 `C13M3-BLOCKER-501`：C13-M3 队列应为空，C13-M2 队列仍从 S4/S5/S6 继续。

## 使用方式

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
```
