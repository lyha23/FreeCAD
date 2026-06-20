# P8-LinkAssemblyRuntime S0 live 基线与旧队列裁决

## 目标

复核当前 repo 的 Link / Assembly / adapter live baseline，裁决旧 P8 子包和 C4-M5 中哪些是已覆盖基线、哪些是 stale queue、哪些仍是本主线 backendGap。S0 只做审计和矩阵更新，不写实现。

## 必读

- `docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/6-20-18-05-P8-LinkAssembly-运行时产品化主线方案.md`
- `docs/CADCore方案/00-CAD-Core抽取方案.md`
- `docs/CADCore3.0/04-【已实现】Link-Assembly-运行时产品化.md`
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线/`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线/`
- `docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线/`
- `docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/`

## 产物

- 更新 `矩阵/p8_link_assembly_runtime_scope_review_matrix.tsv` 的 baseline / backendGap / staleQueue / releaseGate 状态。
- 更新 `矩阵/p8_link_assembly_runtime_blocker_queue.tsv`，只保留真实未关闭 blocker。
- 在主线总入口补一段 S0 结论：哪些旧队列不能直接续跑，哪些 fixture / tests 是 regression baseline。

## 非目标

- 不实现 C++。
- 不采集新 FreeCAD expected。
- 不把旧文档中的 supported claim 直接复制为当前事实；必须用 live code、fixtures、capability 或队列脚本复核。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线
```
