# P8 DistanceTypeExtendedGeometry S6 实现与发布闸门【已实现】

## 目标

消费 S3-S5 的 DTO、mapping、oracle 和 blocker 结论，落 C++、fixtures、focused tests、C ABI capability、docs / TSV 发布。S6 完成后队列为空。

## 发布结论

- `cad-core/src/assembly/joint_solver.cpp` 已补 request-local `Assembly::AssemblyLink` identity-offset subshape marker subset，覆盖 S5 supported-candidate 中非线性 edge / 非平面 face 的 native marker placement 需求；不扩大到 GUI/session、persistent solver state 或非 identity bundled `offsetPlc`。
- `distance_type_extended_geometry` capability 已发布，supported cases 为 13 个：`LineCircle`、`CircleCircle`、`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere`、`PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere`。
- 以上 13 个 expected 已删除 `known_gap` / `backendGap`，`solver_adapter` 与 FreeCAD native oracle 精确对齐；`bbox_delta=0.2` 仅记录 cad-core display primitive bbox 与 FreeCAD exact bbox 的局部容差，不替代 solver parity。
- `PointCurve` 保留 diagnostic / nonGoal：native expected 已采集，但 FreeCAD 仍是 TODO-like plane-of-curve 行为；当前产品裁决为不接受该行为作为 supported。
- cone、line-surface、curve-face 和 `Other` default/TODO representative cases 继续 diagnostic / nonGoal，不发布为 supported；当前产品裁决为不接受 FreeCAD default/TODO branch 作为 CAD Core 公开能力。

## 产品裁决记录

当前裁决：不接受 `PointCurve`、cone、line-surface、curve-face 或 `Other` default/TODO cases 进入 supported capability。

原因：

- `PointCurve` 在 FreeCAD 里走 TODO-like plane-of-curve 行为，不能等同于用户自然理解的点到曲线最近距离。
- cone、line-surface、curve-face 和 `Other` 依赖 default `ASMTPlanarJoint` fallback，没有明确到具体几何类型的距离语义。
- 对这些行为发布 supported 会让前端和用户误以为 CAD Core 已支持完整曲线 / 圆锥 / 默认距离约束，后续真正实现时还要兼容旧错误承诺。
- CAD Core 当前应返回稳定 diagnostic / unsupported，让前端明确提示“不支持该距离类型”，而不是默默套用 FreeCAD 的 TODO/default fallback。

## Blocker 结论

| blocker | S6 结论 |
| --- | --- |
| `DTE-BLOCK-001` | S2 已关闭，S6 保持 full enum coverage gate。 |
| `DTE-BLOCK-002` | S3 已关闭，S6 消费 primitive / radius / scalar evidence。 |
| `DTE-BLOCK-003` | 关闭为 supported：`LineCircle`、`CircleCircle` 通过 active expected parity。 |
| `DTE-BLOCK-004` | 关闭为 supported：`PlaneCylinder`、`PlaneSphere`、`CylinderCylinder`、`CylinderSphere`、`PointCylinder`、`PointSphere` 通过 active expected parity。 |
| `DTE-BLOCK-005` | 关闭为 supported：`PlaneTorus`、`CylinderTorus`、`TorusTorus`、`TorusSphere`、`SphereSphere` 通过 active expected parity；torus radius 按 FreeCAD helper 为 0。 |
| `DTE-BLOCK-006` | 关闭为 diagnostic / nonGoal：产品裁决不接受 `PointCurve` 和 default/TODO representatives 进入 supported；仅在未来产品裁决改变并重新定义具体语义时才可重开。 |
| `DTE-BLOCK-007` | 关闭：expected 覆盖 supported subset 与 diagnostic boundaries，supported expected 不再靠 `known_gap` skip。 |
| `DTE-BLOCK-008` | 关闭：C ABI capability、adapter test、root doc 和矩阵均发布同一 supported / deferred 边界。 |

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd python3 tools/collect_freecad_expected.py --phase c3m6 --check --skip-unsupported
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest -k distance_type
python3 -m unittest tests.test_adapters.CadCoreAdapterTest -k capabilities
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线/矩阵/*.tsv
git diff --check -- cad-core/include/cad_core/assembly cad-core/src/assembly cad-core/tests cad-core/fixtures/c3m6 cad-core/src/adapters/c_api/c_api.cpp docs/FreeCAD几何生态迁移工程-细分/P8-DistanceTypeExtendedGeometry-OndselSolver收口主线
```

提交前最终命令输出为本步骤验收记录；`CadCoreExpectedFixtureTest` 的 remaining skips 只来自仍带 `known_gap` 的 diagnostic expected，不包含 S6 supported cases。

## 非目标

- 不发布 `PointCurve`、cone、line-surface、curve-face 或 `Other` default/TODO cases；这是当前产品裁决，不是待实现 backendGap。
- 不扩大到 GUI/session、persistent solver state 或完整 Assembly transaction。
- 不靠 fixture 名称、bbox、shape 数量或输出修正推断 FreeCAD DistanceType。
