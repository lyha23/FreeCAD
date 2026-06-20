# P8 LinkAssemblyRuntime 工作步骤总入口【已实现】

## 目标

按 `P8-LinkAssembly-运行时产品化主线` 推进 S0-S6。每一步都必须先读本入口、主线方案、总入口和相关矩阵，再决定是否实现、只审计或拆分 blocker。

## 队列顺序

1. `6-20-18-06-【已实现】P8-LinkAssemblyRuntime-S0-live基线与旧队列裁决.md`
2. `6-20-18-07-【已实现】P8-LinkAssemblyRuntime-S1-FreeCAD源码候选矩阵.md`
3. `6-20-18-08-【已实现】P8-LinkAssemblyRuntime-S2-Link账本与ShowElement事务.md`
4. `6-20-18-09-【已实现】P8-LinkAssemblyRuntime-S3-跨文档Hash与Postfix生命周期.md`
5. `6-20-18-10-【已实现】P8-LinkAssemblyRuntime-S4-多层LinkSub与ImportedElementMap.md`
6. `6-20-18-11-【已实现】P8-LinkAssemblyRuntime-S5-AssemblySolver扩展.md`
7. `6-20-18-12-【已实现】P8-LinkAssemblyRuntime-S6-WebRuntime合同冻结.md`

## 必读

- `docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/6-20-18-05-P8-LinkAssembly-运行时产品化主线方案.md`
- `docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/6-20-18-05-P8-LinkAssembly-运行时产品化主线总入口.md`
- `docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/矩阵/*.tsv`
- `docs/CADCore方案/00-CAD-Core抽取方案.md`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/`
- 旧 P8 Assembly / Marker / Distance / JointType 子包的总入口与矩阵。

## 全局非目标

- 不实现 GUI、Workbench、TaskPanel、ViewProvider 或跨请求 session。
- 不让 adapter、前端或输出层承载建模语义。
- 不把旧 fallback、TODO/default branch 或 diagnostic 发布为 supported。
- 不从 cad-core 输出倒推 FreeCAD expected。

## 轻量验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```
