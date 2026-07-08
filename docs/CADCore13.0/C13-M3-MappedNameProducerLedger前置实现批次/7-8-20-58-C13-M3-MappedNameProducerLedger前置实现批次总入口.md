# C13-M3 MappedName Producer Ledger 前置实现批次总入口

## 目标

补齐 C13-M2 S4 暴露的 producer-side ledger 缺口：在 `NamedShape` 生产阶段携带 FreeCAD-equivalent tag / masterTag / op / raw mapped-name provenance，使 runtime 能发布 source-backed `mappedName.raw/canonical`，而不是把 stable token 当成 FreeCAD raw name。

## 入口文件

- README：`README.md`
- 方案：`7-8-20-58-C13-M3-MappedNameProducerLedger前置实现批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前结论

- C13-M2 S4 不能直接实现 codec；缺少 FreeCAD `TopoShape.Tag` / `ElementMap::encodeElementName()` 所需的 producer evidence。
- C13-M3 是前置账本批次，不关闭 C13-M2 S5/S6。
- 完成后应回到 C13-M2 S4 删除 expectedFailure，并继续 child key / mapper id 验证。

## 使用方式

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M3-MappedNameProducerLedger前置实现批次/工作步骤细分 --format markdown
```
