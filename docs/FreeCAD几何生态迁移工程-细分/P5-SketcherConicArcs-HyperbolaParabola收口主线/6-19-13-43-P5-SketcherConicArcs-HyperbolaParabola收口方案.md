# P5 Sketcher Conic Arcs Hyperbola / Parabola 收口方案

## 当前基线

- 方案时间：2026-06-19。
- 当前仓库：`/Users/li/Chili3DProject/FreeCAD`。
- 当前 live 观察：`git status --short` 为空；`cad-core/src/sketcher/*` 已能搜到 `SketchHyperbolaArc`、`SketchParabolaArc`、`ArcOfHyperbola`、`ArcOfParabola` 的解析与建边钩子。
- 未收口点：`cad-core/tests/test_diagnostics.py` 仍把 `sketch-unsupported-hyperbola` 归为 `unsupported_geometry`，`cad-core/fixtures/{mvp,p5}` 仍保留 unsupported hyperbola 输入。也就是说当前更像“实现钩子已进入基线，但 fixture/oracle/诊断口径未闭环”，不是单纯从零实现。

## FreeCAD 依据

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp::GeomArcOfHyperbola::Save/Restore()`：持久化字段包括 `CenterX/Y/Z`、`NormalX/Y/Z`、`MajorRadius`、`MinorRadius`、`AngleXU`、`StartAngle`、`EndAngle`；恢复路径调用 `GC_MakeHyperbola` 和 `GC_MakeArcOfHyperbola(..., Standard_True)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/Geometry.cpp::GeomArcOfParabola::Save/Restore()`：持久化字段包括 `CenterX/Y/Z`、`NormalX/Y/Z`、`Focal`、`AngleXU`、`StartAngle`、`EndAngle`；恢复路径调用 `gce_MakeParab` 和 `GC_MakeArcOfParabola(..., Standard_True)`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp::SketchObject::isSupportedGeometry()`：`Part::GeomArcOfHyperbola` 与 `Part::GeomArcOfParabola` 属于 Sketcher supported geometry。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchAnalysis.cpp::PointConstraints::addGeometry()`：双曲线弧与抛物线弧都通过 start/end 点进入 point constraint 分析。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp::exposeInternalGeometryForType<Part::GeomArcOfHyperbola/GeomArcOfParabola>()`：内部几何暴露包含双曲线主/副轴、焦点与抛物线焦点/焦轴；本轮只把 profile/external geometry 的几何建边和诊断口径收口，不迁移完整 solver 内部约束体系。

## cad-core 落点

- `cad-core/src/sketcher/sketch_object_geometry.*`：解析 `ArcOfHyperbola` / `Part::GeomArcOfHyperbola`、`ArcOfParabola` / `Part::GeomArcOfParabola`，并保留 construction 过滤语义。
- `cad-core/src/sketcher/sketch_object_operations.*`：按 FreeCAD `Restore()` 字段构造 OCCT conic arc edge。
- `cad-core/src/sketcher/sketch_object_external.*`：外部几何合并和计数需要包含两类 conic arc，不能只在 profile 路径可用。
- `cad-core/tests/` 与 `cad-core/fixtures/`：把旧 unsupported hyperbola 口径改成有明确 fixture/oracle 的支持口径，同时保留非法参数诊断。

## 最小完整语义批次

本轮不拆成“先双曲线、后抛物线”。两者在 FreeCAD 同属 `GeomArcOfConic` 派生弧，字段恢复路径、Sketcher supported geometry 判断、profile edge 建边和 external geometry 合并边界一致，应作为一个 P5 conic arcs 批次收口。

批次内至少覆盖：

- Hyperbola arc：有效 profile edge、construction 过滤、非法半径/参数诊断。
- Parabola arc：有效 profile edge、construction 过滤、非法 focal/参数诊断。
- External geometry：两类 conic arc 经外部几何合并后，profile edge 计数和 subshape 输出稳定。
- 诊断口径：旧 `unsupported_geometry` case 改为支持能力或改名为真正 unsupported case，避免测试继续表达过期能力状态。

## 工作包结构

- `工作步骤细分/6-19-13-44-P5CONIC-S0-live基线与边界复核.md`
- `工作步骤细分/6-19-13-45-P5CONIC-S1-FreeCAD源码与当前实现审计.md`
- `工作步骤细分/6-19-13-46-P5CONIC-S2-fixture与oracle矩阵设计.md`
- `工作步骤细分/6-19-13-47-P5CONIC-S3-实现补齐与诊断口径切换.md`
- `工作步骤细分/6-19-13-48-P5CONIC-S4-验证能力发布与提交闸门.md`
- `矩阵/p5_conic_arcs_source_candidates.tsv`
- `矩阵/p5_conic_arcs_scope_review_matrix.tsv`
- `矩阵/p5_conic_arcs_blocker_queue.tsv`
- `矩阵/p5_conic_arcs_non_goal_registry.tsv`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_diagnostics
python3 -m unittest tests.test_mvp
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p6_topology tests.test_expected_fixtures
```

重型收口：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/工作步骤细分 --format markdown
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线 cad-core
```
