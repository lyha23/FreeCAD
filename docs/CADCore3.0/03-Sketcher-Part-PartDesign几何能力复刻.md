# Sketcher / Part / PartDesign 几何能力复刻

## 目标

C3-M3 / C3-M4 / C3-M5 的目标是把 FreeCAD 常用建模语义从“代表 fixture 可跑”推进到“按 Workbench family 可持续复刻”。本主线必须建立在 C3-M1 / C3-M2 的 TopoNaming 与 ExternalGeometry 主路径之上。

## C3-M3：Sketcher 复刻

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/Sketch.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectConstraints.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp`

交付内容：

- solver-facing diagnostics 第一切片已覆盖：同一 target 的 Horizontal / Vertical 冲突、重复 orientation 约束冗余、同一 datum target 不同值冲突、重复 datum 冗余；这些状态在 profile 构造前输出 `sketch_solver_conflict` / `sketch_solver_redundant`，并返回 `profile_ready=false`。
- 后续继续补完整 solver-facing geometry / constraints 数据模型，区分建模几何、construction geometry、external geometry；full DoF、under-constrained、partial redundancy、malformed constraint 和 solver geometry update 仍是剩余 gap。
- 补 ExternalGeometry 的 projection / intersection 复杂路径，包括非简单 planar face、datum、linked target、missing target。
- InternalShape 的 bounded face + open wire 混合场景已有第一切片 oracle；后续补复杂 self-intersection、solver-facing split diagnostics 和更多 inter-edge split 组合。

完成判定：

- Sketcher fixture 不只覆盖 profile 成功，还覆盖 solver state、diagnostics、ExternalGeometry flags update 和 stable subname。
- open profile / open wire 不伪装为 profile-ready face；solver conflict / redundant 不继续生成 fake profile。
- InternalFace / InternalEdge / InternalVertex 仍只由 producer evidence / MapperHistory / diagnostics 解释。

## C3-M4：Part Workbench 复刻

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FeatureOffset.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShape.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeMapper.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/modelRefine.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`

交付内容：

- 系统覆盖 Part primitives：Box、Cylinder、Cone、Sphere、Torus、Plane、Line、Circle、Ellipse 等。
- 系统覆盖 Part operations：Boolean、Extrude、Revolve、Sweep、Loft、Section、Offset、Thickness、Refine、Compound、CompoundFilter。
- `Part::Offset` 面源第一切片已覆盖：`Source`、`Value`、`Mode`、`Join`、`Intersection`、`SelfIntersection`、`Fill=false` 进入 `topo::makeElementOffsetFromSource()`，并通过 maker history 输出 `Plane.Face/Edge/Vertex -> Offset.*`；`Fill=true`、solid source `makeElementSolid()` 恢复、Offset2D 和 Thickness 仍保持显式 capability gap。
- import/export 建立 ElementMap 或明确不可恢复 diagnostics。
- ShapeFix、BOPCheck、invalid shape diagnostics 进入统一 runtime 输出。

完成判定：

- Part feature 不只输出 shape，还输出可追溯 `NamedShape`、subshape map、ElementMap / MapperHistory 或明确 diagnostics。
- import shape 的拾取引用能在可恢复范围内跨 recompute 保持稳定。
- OCCT 失败、退化、空 shape、unsupported format 都有稳定 diagnostics。

## C3-M5：PartDesign 完整生态

FreeCAD 依据：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/Feature.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePad.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePocket.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureDressUp.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp`

交付内容：

- full transformed / pattern history 已覆盖第一批主路径：Mirror、LinearPattern、PolarPattern、Scaled、MultiTransform 的 Features / WholeShape、multi-original、chain DressUp、refined support 与 Link retag 组合。
- DressUp 复杂参数：Fillet / Chamfer 的 Edge / Face 多选择第一片已记录请求 subname 到实际 EdgeN 的展开证据；Chamfer `Two distances` / `Distance and Angle`、Draft no-face copy / 显式 DatumPlane-Line / 自动 neutral-plane guess、PartDesign Thickness no-face copy / FaceN Mode-Join 参数变体 / multi-solid fuse history 已有结构化 fixture；empty / invalid / unsupported Base selection 与 invalid parameter 失败诊断已有结构化 fixture；chain DressUp + Pattern history 已覆盖 SupportTransform AddSubShape cache、source-prefixed alias 和 terminal split/deleted 传播；后续继续补更完整引用恢复。
- Body chain ownership 第一片已覆盖外部 `Body.BaseFeature` 触发内部 `PartDesign::FeatureBase` 创建、`Body.Group` 头部同步、首个 solid feature `BaseFeature` 回写，以及删除旧 Tip 后 previous / next `Body.Tip` 与下一 solid feature `BaseFeature` reroute；这些写回都保持 request graph immutable。
- Hole 完整表驱动第一片已覆盖：ThreadType / ThreadSize 表驱动直径、`Resources/Hole` head cut definition、ISO / DIN 动态 head cut、ModelThread pipe-shell 几何，以及 `findHoles()` subtractive Body cut history；后续继续冻结完整 ElementMap cut history 与更多 profile / head cut 组合。
- 补 Revolution、Groove、Loft、Pipe、Boolean、Datum attachment 的常用 FreeCAD oracle。
- Body chain ownership 的 BaseFeature writeback、Tip delete/reroute、Origin placement、Origin datum relink 与 Add/Sub replay stop-at-tip 已有第一片；后续再补 visibility / group 扩展语义。

完成判定：

- 常用 PartDesign workflow 可以长期编辑：修改 sketch、切换参数、重算 Body、下游引用仍可恢复或稳定诊断。
- transformed / pattern 不靠 instance index、bbox、面积或输出顺序猜 source ownership。
- unsupported 参数组合明确进入 capabilities / diagnostics，而不是静默失败。

## 验收命令

本主线代码修改后优先执行：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_feature_flows tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features
```

阶段收口时补：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures
```
