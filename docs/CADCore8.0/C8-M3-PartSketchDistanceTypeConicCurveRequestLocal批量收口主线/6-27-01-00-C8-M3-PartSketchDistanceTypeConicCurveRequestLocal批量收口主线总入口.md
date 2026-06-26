# C8-M3 Part / Sketch / DistanceType Conic Curve Request-Local 批量收口主线总入口

## 主线目标

C8-M3 不继续扩展已关闭的 C8-M2 `SubShapeBinder CopyOnChange` 包，而是从 live capability 的 active gap 出发，对 `part_workbench.conic_curves` 做一轮批量收口。

目标不是单独删除一个 `remaining_gaps` 字符串，而是把同一条 conic 曲线语义链按 request-local CAD Core 边界闭环：

- `PartConicCurveDTO`：Hyperbola / Parabola finite edge producer、metadata、diagnostics 和 Part consumer。
- `Sketcher`：ArcOfHyperbola / ArcOfParabola 的几何输入、profile / external-reference 证据和 solver-facing 边界。
- `DistanceType`：默认 / todo 分类是否还能保留为 active gap，或应被实现 / 重分类为 conic reference boundary。
- `GUI / full solver`：明确 non-goal、delete/reopen condition 和前端 / CAD Core 行为。

## 当前基线

- S0 live 基线已冻结：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=c6a848b69c`（`c6a848b69c docs: 完成 C8-M2 S6 发布闸门`）。
- S0 开始状态显示 `docs/CADCore8.0/README.md` 已修改，以及本 C8-M3 文档包 / 矩阵 / 工作步骤未跟踪；未见 C++、Rust、fixture、expected、collector 或无关源码 dirty 文件。
- C8-M2 队列为空，C8-M3 队列首项为 S0；S0 完成后下一 pending 是 S1。
- `cad-core capabilities` 当前显示 `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]`，但 C8-M2 已裁决为 no-code release gate。
- `part_workbench.conic_curves.status=done_part_geometry_curve_edge_consumer`。
- `part_workbench.conic_curves.remaining_gaps=["gui_conic_edit","full_sketcher_solver_conic_constraints","distance_type_default_todo"]` 仍作为 C8-M3 输入保留，S0 不删除或声明已支持。
- 现有 expected-backed 证据包括 `p8/part-hyperbola-edge`、`p8/part-parabola-edge`、`p8/part-conic-edge-invalid-params`、`p8/part-conic-edge-extrusion`、`p8/part-ruled-surface-conic-line`。

## 为什么同轮批量处理

`Part.Hyperbola` / `Part.Parabola`、Sketcher conic arcs 和 DistanceType 的默认分类都围绕同一个 typed conic reference / curve API 边界。如果只处理 `distance_type_default_todo` 或只把 GUI 写成 non-goal，会留下一个薄包，后续仍会在同一条 conic DTO 线上反复开单 fixture。C8-M3 因此把以下代表场景纳入同一轮：

- Part request-local conic edge producer current coverage 复核。
- 至少一批 Part consumer / surface consumer 代表场景复核或扩展裁决。
- Sketcher conic geometry 输入 / profile / external reference 证据复核。
- DistanceType default / curve reference 分类 native 与 cad-core 对齐。
- GUI edit 与 full solver conic constraints 的 non-goal 发布。

若 S2 发现某项不适合同轮实现，必须在矩阵中写明拆分原因、下一批范围和防止单 fixture 推进的闭环。

## 证明链条

```text
S0 live 基线与声明冻结
  -> S1 FreeCAD source / current coverage 批量复核
  -> S2 scope / blocker / non-goal / implementation gate 分类
  -> S3 PartConicCurveDTO producer-consumer oracle 批量复核
  -> S4 Sketcher conic 输入与 solver-facing 边界复核
  -> S5 DistanceType default / conic reference 分类实现准入
  -> S6 实现 / 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| 3D conic geometry | `src/Mod/Part/App/Geometry.cpp::GeomHyperbola/GeomParabola/GeomArcOf*::Save/Restore()` | 持久化 `MajorRadius` / `MinorRadius` / `Focal` / `AngleXU` / trim 参数 |
| 2D conic geometry | `src/Mod/Part/App/Geometry2d.cpp::Geom2dHyperbola/Geom2dParabola/Geom2dArcOf*::Save/Restore()` | Sketcher / surface 2D conic 输入的字段依据 |
| Sketcher external conic | `src/Mod/Sketcher/App/SketchObjectExternal.cpp` | `GeomAbs_Hyperbola` / `GeomAbs_Parabola` 投影后构造 Part conic geometry |
| Sketcher conic status | `src/Mod/Sketcher/App/Constraint.h`、`SolverGeometryExtension.h`、`GeoList.cpp`、`SketchAnalysis.cpp` | conic constraints / solver-facing 状态存在，但 full solver 不属于本轮 CAD Core 实现目标 |
| DistanceType | `src/Mod/Assembly/App/AssemblyUtils.cpp::getDistanceType()`、`AssemblyObject.cpp::makeMbdJointOfType()` | 根据 reference geometry 分类 PointCurve / Other 等 solver DTO |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| Part conic DTO | `cad-core/src/part/part_geometry_curve.cpp` | `PartConicCurveDTO` parse / edge build / metadata / consumer bridge |
| Part consumers | `cad-core/src/part/part_extrusion.cpp`、`cad-core/src/part/part_ruled_surface.cpp` | 已验证 conic edge -> Part consumer，S3 裁决是否扩展代表场景 |
| Sketcher conic input | `cad-core/src/sketcher/sketch_object_geometry.cpp`、`sketch_object_operations.cpp`、`sketch_object_external.cpp` | ArcOfHyperbola / ArcOfParabola 输入、profile、external references |
| DistanceType | `cad-core/src/assembly/joint_solver.cpp`、`cad-core/src/assembly/assembly_utils.cpp` | reference classification 和 solver DTO 输出 |
| capability/tests | `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_adapters.py`、`test_p8_features.py`、`test_p5_sketch.py` | 发布口径、fixtures、focused validation |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| README | `README.md` | 包入口 |
| 方案 | `6-27-01-00-C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口方案.md` | 实施方案 |
| 工作步骤总入口 | `工作步骤细分/6-27-01-00-【已实现】C8-M3工作步骤总入口.md` | 队列索引 |
| S0 | `工作步骤细分/6-27-01-01-【已实现】C8-M3-S0-live基线与批量范围冻结.md` | 声明与基线 |
| S1 | `工作步骤细分/6-27-01-02-C8-M3-S1-FreeCAD源码与current覆盖批量复核.md` | source authority |
| S2 | `工作步骤细分/6-27-01-03-C8-M3-S2-scope准入与blocker矩阵.md` | route 分类 |
| S3 | `工作步骤细分/6-27-01-04-C8-M3-S3-PartConicCurveDTO生产消费oracle批量复核.md` | Part DTO / consumer |
| S4 | `工作步骤细分/6-27-01-05-C8-M3-S4-SketcherConic输入与solver边界复核.md` | Sketcher 边界 |
| S5 | `工作步骤细分/6-27-01-06-C8-M3-S5-DistanceType默认分类与capability准入.md` | DistanceType / capability |
| S6 | `工作步骤细分/6-27-01-07-C8-M3-S6-实现与发布闸门.md` | implementation / no-code gate |
| source candidates | `矩阵/c8m3_conic_requestlocal_source_candidates.tsv` | FreeCAD / cad-core source |
| scope review | `矩阵/c8m3_conic_requestlocal_scope_review_matrix.tsv` | scope / status |
| blocker queue | `矩阵/c8m3_conic_requestlocal_blocker_queue.tsv` | blocker / close condition |
| non-goal | `矩阵/c8m3_conic_requestlocal_non_goal_registry.tsv` | GUI / full solver / fake TypeId |
| backend gap | `矩阵/c8m3_conic_requestlocal_backend_gap_classification.tsv` | implementation gate |
| oracle plan | `矩阵/c8m3_conic_requestlocal_oracle_plan.tsv` | native/current expected plan |
| validation | `矩阵/c8m3_conic_requestlocal_validation_matrix.tsv` | 验收命令 |

当前 S0 已完成 live 基线与批量范围冻结，`C8M3-BLOCKER-000` 已关闭为 `closed_S0_live_baseline_and_batch_scope_frozen`；S1-S6 仍为待执行状态。矩阵中 S0 范围、non-goal 和 blocker seed row 已冻结，后续行不是发布闸门结论。
