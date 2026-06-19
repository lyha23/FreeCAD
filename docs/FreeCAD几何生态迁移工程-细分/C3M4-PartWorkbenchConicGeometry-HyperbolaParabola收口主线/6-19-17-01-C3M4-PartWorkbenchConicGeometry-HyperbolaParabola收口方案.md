# C3M4 Part Workbench Conic Geometry Hyperbola / Parabola 收口方案

## 当前基线

- 方案时间：2026-06-19。
- 当前仓库：`/Users/li/Chili3DProject/FreeCAD`。
- S0 live 基线：`pwd` 为 `/Users/li/Chili3DProject/FreeCAD`；HEAD 为 `6f70a6ad6a`（`6f70a6ad6a feat: 收口P5圆锥弧草图支持`）。
- S0 脏工作区边界：当前已有 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp` 改动，以及本 C3M4 主线文档 / 矩阵未跟踪文件；本主线保护这些现有改动，不执行 reset / checkout / 回退。
- S0 queue 结论：P5CONIC `step_goal_queue.py` 返回空队列；PARTCONIC queue 在 S0 收口前从 `6-19-17-02-PARTCONIC-S0-live基线与范围冻结.md` 开始，S0 完成后下一项应为 S1。
- 上一轮已收口：P5CONIC 已发布 `ArcOfHyperbola` / `ArcOfParabola` 的 Sketcher profile、construction 过滤、native `ExternalGeo` 与 projected `ExternalGeometry` 支持。
- 新主线定位：从 Sketcher conic arcs 向 Part workbench / Part geometry API 侧推进，补齐 `Part.Hyperbola` / `Part.Parabola` 作为 Part 几何对象进入 cad-core 计算和消费链路的能力。
- 关键边界：当前 FreeCAD 源码里没有 `Part::Hyperbola` / `Part::Parabola` 这种 `DocumentObject` primitive；它们是 Python 暴露的 `Part.Hyperbola` / `Part.Parabola` geometry wrapper 与 `Part::Geom*` 类型。因此本方案先定义“Part geometry curve DTO / shape conversion / consumer”路径，不伪造不存在的 FreeCAD `TypeId`。

## 为什么接这条主线

- P5CONIC 已把 Sketcher 侧圆锥弧支持闭环，但能力文档明确排除了未进入本轮的 Part workbench conic surface / Part geometry 侧能力。
- cad-core 目前已有 Part primitive executor：`Part::Line`、`Part::Ellipse`、`Part::Cone`、`Part::Torus`、`Part::Extrusion`、`Part::Section` 等；但没有独立的 Part hyperbola / parabola 曲线请求入口。
- FreeCAD Part 层已经有完整的 `GeomHyperbola` / `GeomParabola`、arc wrapper、Python 构造器、TopoShape edge curve extraction 依据；这是比直接进入 Sketcher solver 内部约束更小且更清晰的语义批次。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp::GeomHyperbola::Save/Restore()`：持久化字段为 `CenterX/Y/Z`、`NormalX/Y/Z`、`MajorRadius`、`MinorRadius`、`AngleXU`；恢复路径通过 `GC_MakeHyperbola` 建立 `Geom_Hyperbola`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp::GeomArcOfHyperbola::Save/Restore()`：在 Hyperbola 字段基础上增加 `StartAngle` / `EndAngle`，恢复后调用 `GC_MakeArcOfHyperbola(..., Standard_True)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp::GeomParabola::Save/Restore()` 与 `GeomArcOfParabola::Save/Restore()`：字段为 `CenterX/Y/Z`、`NormalX/Y/Z`、`Focal`、`AngleXU`、`StartAngle`、`EndAngle`，恢复路径使用 `gce_MakeParab` / `GC_MakeArcOfParabola(..., Standard_True)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/HyperbolaPyImp.cpp`、`ParabolaPyImp.cpp`、`ArcPyImp.cpp`、`ArcOfHyperbolaPyImp.cpp`、`ArcOfParabolaPyImp.cpp`：Python API 支持 `Part.Hyperbola` / `Part.Parabola` 与有限参数范围 arc 构造。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeEdgePyImp.cpp::getCurve()`：`GeomAbs_Hyperbola` / `GeomAbs_Parabola` edge 会被还原成 `HyperbolaPy` / `ParabolaPy`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/PrimitiveFeature.cpp`：存在 `Part::Line`、`Part::Ellipse`、`Part::Cone` 等 primitive `DocumentObject`，但没有 Hyperbola / Parabola primitive；这是本轮 DTO 边界的依据。

## cad-core 落点

- `cad-core/src/part/primitive_feature.cpp` 与 `cad-core/include/cad_core/part/part_feature.h`：只在 S1 确认存在 FreeCAD primitive 依据时才新增 executor；若确认仍是 Python geometry API，则不要注册假的 `Part::Hyperbola` / `Part::Parabola`。
- `cad-core/src/part/part_feature_support.*` 或新增 `cad-core/src/part/part_geometry_curve.*`：承接 Hyperbola / Parabola 到有限 `TopoDS_Edge` 的通用构造 helper。
- `cad-core/tools/collect_freecad_expected.py`：新增 FreeCAD oracle 构造路径，使用 `Part.Hyperbola` / `Part.Parabola` + `Part.ArcOf*` / `toShape()` 采集 edge、bbox、subshape 和 curve metadata。
- `cad-core/fixtures/p8` 与 `cad-core/tests/test_p8_features.py`：沿用现有 Part primitive fixture/test 线，新增 Part geometry conic fixtures；如果 S1 决定拆成 C3M4 专用 fixture 目录，必须同时更新 runner 与 expected fixture 发现规则。
- `cad-core/src/part/part_extrusion.cpp`：作为第一批 consumer 裁决点，验证 conic edge 能否被 Part Extrusion 稳定消费并形成 face/shell；不把 RuledSurface / ProjectionOnSurface 拉进第一轮。
- `docs/CADCore3.0/03-【已实现】Sketcher-Part-PartDesign几何能力复刻.md` 与 `docs/CADCore3.0/capabilities-gap对照表.md`：只在 S5 验证通过后发布能力，不能提前把完整 conic surface family 写成 supported。

## 最小完整语义批次

本轮不拆成“先 Hyperbola、后 Parabola”。两者在 FreeCAD Part geometry 层同属 conic curve wrapper，字段恢复、finite arc、edge extraction、invalid 参数和 Part consumer 的风险边界一致，应作为一个 Part conic geometry 批次推进。

批次内至少覆盖：

- Hyperbola finite edge：center、normal、major/minor radius、angleXU、start/end 参数构造出稳定 `TopoDS_Edge`。
- Parabola finite edge：center、normal、focal、angleXU、start/end 参数构造出稳定 `TopoDS_Edge`。
- Invalid 参数：半径、focal、trim range、non-finite 参数进入明确 diagnostics，不落到输出端修剪。
- Part consumer：至少一个 Part workbench consumer 能消费 conic edge；优先用 `Part::Extrusion` 证明 edge 到 face/shell 的通路。
- Topo extraction：从 result edge 读取 `GeomAbs_Hyperbola` / `GeomAbs_Parabola` metadata，subshape / named shape 稳定。

## 非目标

- 不迁移完整 Sketcher solver 内部辅助几何 / conic 约束。
- 不实现 GUI conic edit。
- 不伪造 FreeCAD 不存在的 `Part::Hyperbola` / `Part::Parabola` `DocumentObject`。
- 不在本批次承诺完整 Part surface family、RuledSurface、ProjectionOnSurface、DistanceType default / TODO 分支。
- 不把 Hyperbola / Parabola 转成 polyline、BSpline 或 BREP fixture 来绕过正式 conic 类型。

## 工作包结构

- `矩阵/part_conic_geometry_source_candidates.tsv`
- `矩阵/part_conic_geometry_scope_review_matrix.tsv`
- `矩阵/part_conic_geometry_blocker_queue.tsv`
- `矩阵/part_conic_geometry_non_goal_registry.tsv`
- `工作步骤细分/6-19-17-02-PARTCONIC-S0-live基线与范围冻结.md`
- `工作步骤细分/6-19-17-03-PARTCONIC-S1-FreeCAD源码与DTO边界审计.md`
- `工作步骤细分/6-19-17-04-PARTCONIC-S2-fixture与oracle矩阵设计.md`
- `工作步骤细分/6-19-17-05-PARTCONIC-S3-conic曲线edge构造实现.md`
- `工作步骤细分/6-19-17-06-PARTCONIC-S4-Part消费者与surface裁决.md`
- `工作步骤细分/6-19-17-07-PARTCONIC-S5-能力发布与提交闸门.md`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures
```

重型收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_diagnostics tests.test_mvp tests.test_p5_sketch tests.test_p8_features tests.test_expected_fixtures
```
