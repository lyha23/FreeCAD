# 【已实现】C5-M12-S1 FreeCAD native oracle probe 矩阵

状态：`【已实现】`

完成记录：`../docs/temp/6-22-02-29-C5-M12-S1-FreeCADNativeOracleProbe矩阵记录.md`

## 目标

一次性探测 Sweep、Loft、Filling、GeomPlate 四组 Part Workbench surface helper / wrapper 的 FreeCADCmd collectability，输出 collectable / diagnostic-only / native-hidden / FreeCADCmd blocker 分流矩阵。S1 不允许只探测一个 Sweep fixture。

## 必读

- S0 完成后的本包矩阵。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PartFeatures.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/`
- `cad-core/tools/collect_freecad_expected.py`

## 产物

- Probe 记录或 collector draft，覆盖四组 surface owner。
- 明确哪些场景能在 S2-S4 直接 expected-backed，哪些只能保留 blocker。
- 更新 `C5M12-BLK-101`、`C5M12-SCOPE-101`、`C5M12-ORC-101`。

## 非目标

- 不替换 expected。
- 不做 capability promotion。
- 不修改 FreeCAD upstream source。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵 cad-core/tools/collect_freecad_expected.py
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M12-PartWorkbenchSurfaceNativeOracleRecovery主线/工作步骤细分 --format markdown
```
