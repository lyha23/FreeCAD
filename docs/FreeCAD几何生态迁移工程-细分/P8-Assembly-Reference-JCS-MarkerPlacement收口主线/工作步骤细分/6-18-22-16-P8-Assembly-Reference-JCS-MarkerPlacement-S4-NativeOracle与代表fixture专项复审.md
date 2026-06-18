# P8 Assembly Reference / JCS MarkerPlacement S4 NativeOracle 与代表 fixture 专项复审

## 目标

为同一 marker placement 语义批量采集 representative FreeCAD expected，不再只挑单个 Distance fixture。

## 代表 fixture 批次

| fixture 组 | 覆盖 |
| --- | --- |
| object baseline | 既有 native placement expected 保持通过 |
| vertex | Ball vertex-vertex；Distance PointPoint zero/nonzero |
| edge | Revolute edge-edge；Slider edge-edge；Cylindrical edge-edge；Distance LineLine / PointLine |
| face | Fixed face-face；Parallel / Perpendicular / Angle face-face；Distance PlanePlane / PointPlane / LinePlane |
| mixed + swapped | PointLine、PointPlane、LinePlane 原始输入顺序与 swap 后顺序 |
| current value | Distance / Angle 代表 case 中保留 Placement1/2 乘 object/subshape global placement 后得到 scalar 或 diagnostic 的证据 |
| special rewrite | RackPinion marker rewrite；Screw sliding-side swap；Gears / Belt scalar marker 回归 |

## collector 要求

- 优先复用两个 `Part::Box` + `Assembly::AssemblyLink` + GroundedJoint + target Joint 的 c3m6 shape。
- 对新增 fixture 做单项 `--check`，不借 `--phase c3m6 --check` 的 unrelated historical failure 判断本包。
- expected 中必须保留 `placement_updates`、solver adapter status、solver joint evidence。
- Vertex / Edge / Face / mixed 必须作为同一 oracle 批次推进；如果某一类因 FreeCADCmd 或 fixture 构造阻塞，S4 必须写清阻塞原因、下一批次范围，以及为什么不能让 S5 只收一个 case。

## 验收

```bash
python3 cad-core/tools/collect_freecad_expected.py <fixture> --out <expected> --check --freecadcmd /Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd
python3 -m unittest cad-core.tests.test_p8_features.CadCoreP8FeatureTest -k marker
git diff --check -- cad-core/fixtures/c3m6 cad-core/tests docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Reference-JCS-MarkerPlacement收口主线
```

## 非目标

- 不刷新 unrelated c3m6 expected。
- 不把 expected mismatch 归因到 FreeCAD 前先核对 fixture 构造。
- 不以一个 primitive 或一个 Distance fixture 代表整个 marker placement 语义。
