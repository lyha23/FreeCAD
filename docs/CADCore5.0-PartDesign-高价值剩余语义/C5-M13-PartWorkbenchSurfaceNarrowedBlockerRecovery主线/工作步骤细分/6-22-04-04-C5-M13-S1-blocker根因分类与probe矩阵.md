# C5-M13-S1 blocker 根因分类与 probe 矩阵

状态：`pending_C5M13-S1_blocker_probe_matrix`

## 目标

对 S0 冻结的 Sweep / Filling / GeomPlate narrowed blockers 做一次 FreeCADCmd root-cause probe。S1 的产物不是实现，而是分类矩阵：collector-fixable、representative-shape-fixable、native-runtime blocker、native-hidden diagnostic-only、FreeCAD NotImplemented 或 non-goal。

## 必读

- S0 完成后的局部矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- C5-M12 的 `docs/temp/6-22-02-29-C5-M12-S1-FreeCADNativeOracleProbe矩阵记录.md`
- 相关 FreeCAD 源码：PipeShell wrapper、Filling helper、GeomPlate helper。

## 产物

- `docs/temp/` 下新增 C5-M13 probe 记录，包含每个代表场景的命令、输入形态、输出 / 错误、分类、delete condition。
- 如果新增临时 probe 脚本，放在本包 `docs/temp/`，不要混入 cad-core 源码。
- 更新 `C5M13-BLK-101`、`C5M13-SCOPE-101`、`C5M13-ORC-101` 和 root `C5-ORC-1302`。
- 为 S2-S4 指定可 expected-backed 场景和必须保留 blocker 的场景。

## 非目标

- 不替换 expected。
- 不放宽 tests。
- 不把 runtime crash 解释为 cad-core support。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M13-PartWorkbenchSurfaceNarrowedBlockerRecovery主线/工作步骤细分 --format markdown
```
