# C5-M9-S0 live 基线与 scope 冻结

状态：`pending`

## 目标

冻结 C5-M9 的 live baseline，证明本包只打开 `Part::ProjectOnSurface` projected subshape provenance / mapper history 第二批，不重做 C4M1 已 expected-backed 的投影几何。

## 必读

- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/6-21-19-25-C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批方案.md`
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/矩阵/*.tsv`
- `docs/CADCore3.0/capabilities-gap对照表.md`
- `docs/CADCore4.0/C4-M1-PartWorkbenchSurface-ProjectOnSurface独立主线/`
- `cad-core/src/part/part_project_on_surface.cpp`
- `cad-core/tests/test_p8_features.py`

## 产物

- 记录当前 `pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git status --short -uall`。
- 复核 12 个 `cad-core/fixtures/c4m1/part-project-on-surface-*` fixture 与 adapter capability 的当前状态。
- 必要时只修正文档/矩阵措辞，让 root row、local matrix 和 README 对 C5-M9 范围一致。
- 更新本 step 文件名为 `【已实现】...`，并在局部 blocker queue 中关闭 `C5M9-BLK-000`。

## 非目标

- 不写 cad-core 实现。
- 不采集新 expected。
- 不关闭 `projected_edge_provenance_mapper_history` gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M9-PartWorkbenchProjectOnSurfaceProvenance第二批主线/工作步骤细分 --format markdown
```
