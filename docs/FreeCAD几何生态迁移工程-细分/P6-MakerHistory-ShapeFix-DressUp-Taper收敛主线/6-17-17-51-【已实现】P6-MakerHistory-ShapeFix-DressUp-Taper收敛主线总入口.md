# P6 MakerHistory / ShapeFix / DressUp / Taper 收敛主线总入口

本文是 `docs/FreeCAD几何生态迁移工程-细分` 下的 P6 MakerHistory 余量收敛主线。

对应上游方案入口是 `docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md`、`docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md` 和 `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md`。

## 主线目标

- 复核 P6/P7/P8 文档、C ABI capabilities 和 focused tests 对 ShapeFix、DressUp、transformed、taper history 的状态是否一致。
- 把真正仍未关闭的 MakerHistory producer 缺口落到 `topo` / `part` / `part_design` 正式账本，不在 executor、adapter 或 expected 输出端做 fixture 补丁。
- 对当前已标为 covered 的能力建立发布闸门：若代码和测试已支持，回写方案状态；若 FreeCAD oracle 或 focused test 证明 mismatch，再转为 `backendGap` 并进入 S6 C++ 实现。

## 当前基线

- S0 live 复核确认：此前 `09-P6-TopoNaming主路径.md` 与 `00-CAD-Core完整抽取执行总览.md` 对 ShapeFix、DressUp、transformed copy 和 taper history 存在硬缺口措辞；本轮已改成 releaseGate / S3-S5 待裁决口径，不再把 taper 写成必然 `known_gap` 或把 ShapeFix / DressUp 写成必然未实现。
- 当前 `cad-core/src/adapters/c_api/c_api.cpp` 已把 `taper_history` 标为 `covered_full`，`shape_fix` 标为 `covered_no_generated_producer` 且 `remaining=[]`，`dressup` 标为 `done_first_slice` 且 `remaining=[]`，`transformed` 标为 `covered`。
- `cad-core/tests/test_adapters.py` 已断言 `taper_full_history`、`shapefix_history`、`shapefix_modified_generated_history`、`transformed_pattern_full_history` 不在 remaining gaps 中。
- `10-P7-PartDesign常用生态.md` 与 `11-P8-Part导入导出与Assembly后续.md` 已保留当前 covered 子集和复杂后续边界，没有把 P6 MakerHistory releaseGate 误升级为 backendGap。
- 因此本主线第一步不是直接写 C++，而是消解 live 状态漂移：文档、capabilities、fixtures 和 FreeCAD source authority 必须先对齐。

## 总入口复核状态

- 2026-06-17 S0 live 基线复核已完成：S0-S6 文件索引、矩阵文件名、状态词典和 TSV 列骨架可作为 live seed 执行。
- 2026-06-17 S1 FreeCAD 源码候选矩阵已复核：`p6_maker_history_source_candidates.tsv` 已补成 10 条候选，覆盖 ShapeFix、DressUp、Refine、taper、transformed 和 capability / test 发布口径。
- 2026-06-17 S2 范围准入与 blocker 矩阵已完成：当前分类为 4 个 `releaseGate`、1 个 `notCollected`、0 个 `backendGap`，`P6MH-NG-001..005` 保留为明确非目标。
- 2026-06-17 S3 ShapeFix History 专项复审已完成：`P6MH-SCOPE-002` 为 `supported`，ShapeFix deleted / modified history 有实现和 focused tests，`generated_empty_review` 不是 backendGap。
- 2026-06-17 S4 DressUp / Refine 传播专项复审已完成：`P6MH-SCOPE-003` 为 `supported`，AddSubShape slot、RefineModel 和 transformed / pattern 传播有 focused tests，完整 GUI 参数全集仍不在本主线扩大范围内。
- 2026-06-17 S5 taper partial/full history 专项复审已完成：`P6MH-SCOPE-004` 为 `supported`，当前 object metadata / capability / tests 不再暴露 `known_gap:taper_history`；旧 P3b 文档残留已由 S6 发布回写关闭。
- 2026-06-17 S6 发布闸门已完成：P3b、P6 和总览文档已回写到 supported 口径，当前无 C++ backendGap；复杂 split / deleted 旧引用恢复保留为 `notCollected` oracle 队列。
- 本次复核只确认执行索引、分类矩阵和闸门纪律，不把 `releaseGate` / `notCollected` 直接升级为 `backendGap`，也不宣称整条 P6 主线已实现。

## 证明链条

```text
声明口径与 live 基线
  -> FreeCAD 源码候选
  -> scope / blocker / nonGoal 分类
  -> ShapeFix producer 专项复审
  -> DressUp / Refine / transformed 传播专项复审
  -> taper partial/full history 专项复审
  -> oracle / C++ 实现 / 发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| ShapeFix history mapper | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::MapperHistory` | `MapperHistory(ShapeFix_Root&)` 通过 ShapeFix / ReShape history 读取 modified / generated |
| ShapeFix Python producer | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::ShapeFixModule::removeSmallEdges()` | 调用 `ShapeFix::RemoveSmallEdges(sh, tol, reshape)`，ReShape 是 history 证据 |
| DressUp AddSub cache | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp::DressUp::getAddSubShape()` | `SupportTransform` 跳过连续 DressUp，输出 add/sub compound cache |
| Fillet / Chamfer replacement | `/home/user/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureFillet.cpp`、`FeatureChamfer.cpp` | 调 `makeElementFillet()` / `makeElementChamfer()`，失败或无效时 ShapeFix tolerance，之后 refine |
| taper ThruSections | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/ExtrusionHelper.cpp`、`TopoShapeExpansion.cpp::MapperThruSections` | taper 通过 `BRepOffsetAPI_ThruSections`，`GeneratedFace(s)` / first / last section 参与 history |
| transformed copy | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementTransform()` | transform 后 `copyElementMap(tmp, op)`，不靠几何猜 source |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| topo history | `cad-core/src/part/topo_shape.cpp`; `cad-core/include/cad_core/part/topo_shape.h` | NamedShape / ElementMap / maker history / terminal split-deleted-merge |
| ShapeFix | `cad-core/src/part/shape_fix.cpp`; `cad-core/include/cad_core/part/shape_fix.h` | ShapeFix_Shape / ShapeBuild_ReShape history wrapper |
| taper | `cad-core/src/part/extrusion_helper.cpp`; `cad-core/src/part/part_extrusion.cpp`; `cad-core/src/part_design/feature_extrude.cpp` | Taper geometry、ThruSections maker、Pad / Pocket / Part::Extrusion history metadata |
| DressUp | `cad-core/src/part_design/feature_dress_up.cpp`; `cad-core/src/part_design/feature_dress_up_support.h` | Fillet / Chamfer / Draft / Thickness、SupportTransform、AddSubShape slot history |
| capabilities | `cad-core/src/adapters/c_api/c_api.cpp`; `cad-core/tests/test_adapters.py` | 发布当前 maker history 覆盖和 remaining gaps |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| 工作步骤总入口 | `工作步骤细分/6-17-17-51-【已实现】P6-MakerHistory工作步骤总入口.md` | S0-S6 执行索引 |
| S0 声明口径 | `工作步骤细分/6-17-17-52-【已实现】P6-MakerHistory-S0-声明口径与live基线复核.md` | 冻结 live 状态、禁止声明和状态词典 |
| S1 源码候选 | `工作步骤细分/6-17-17-53-【已实现】P6-MakerHistory-S1-FreeCAD源码候选矩阵.md` | 生成 FreeCAD / cad-core 候选证据 |
| S2 范围准入 | `工作步骤细分/6-17-17-54-【已实现】P6-MakerHistory-S2-范围准入与blocker矩阵.md` | 分类 scope、blocker、backendGap 和 nonGoal |
| S3 ShapeFix | `工作步骤细分/6-17-17-55-【已实现】P6-MakerHistory-S3-ShapeFix-History专项复审.md` | 复核 ShapeFix / ReShape producer 是否还存在 C++ 缺口 |
| S4 DressUp | `工作步骤细分/6-17-17-56-【已实现】P6-MakerHistory-S4-DressUp-Refine-传播专项复审.md` | 复核 DressUp、Refine、transformed 链路 history 传播 |
| S5 taper | `工作步骤细分/6-17-17-57-【已实现】P6-MakerHistory-S5-Taper-Partial-History专项复审.md` | 复核 taper partial/full history 状态漂移和 oracle |
| S6 发布闸门 | `工作步骤细分/6-17-17-58-【已实现】P6-MakerHistory-S6-Oracle实现与发布闸门.md` | 消费 blocker，落 C++ / expected / docs 发布 |
| source candidates | `矩阵/p6_maker_history_source_candidates.tsv` | FreeCAD source 候选 |
| scope review | `矩阵/p6_maker_history_scope_review_matrix.tsv` | 语义项状态矩阵 |
| blocker queue | `矩阵/p6_maker_history_blocker_queue.tsv` | 可执行 blocker 队列 |
| non-goal registry | `矩阵/p6_maker_history_non_goal_registry.tsv` | 非目标与 reopen 条件 |
| backend gap classification | `矩阵/p6_maker_history_backend_gap_classification.tsv` | backendGap / releaseGate 聚合 |

当前 S0-S6 已完成：5 个 scope 为 `supported`，`P6MH-SCOPE-005` 保留 `notCollected`，当前无 `backendGap`。矩阵是本主线发布闸门结论；若后续要关闭复杂 split / deleted 恢复，必须先补 FreeCAD oracle 或 focused mismatch 证据。
