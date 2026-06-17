# P5b：Sketch open-wire / WireJoiner 迁移状态

P5b 负责把 Sketch internal geometry 从 closed-wire baseline 推向 FreeCAD `SketchObject::buildInternals()` 路径：

```text
SketchObject::buildInternals()
  -> TopoShape::makeElementFace(..., "Part::FaceMakerBuildFace")
  -> FaceMakerBuildFace / FaceMaker::postBuild()
  -> WireJoiner::getOpenWires()
  -> InternalShape + InternalFace/InternalEdge/InternalVertex + internal element map
```

## 当前基线

- `features/sketch_object.cpp` 已通过 `geometry::buildSketchInternals()` 发布 request-local `InternalShape`，并把 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN` 合并进 Sketch 的 `subshapes`。
- `geometry/face_maker.*` 已覆盖 bounded split 子集：closed profile wires、closed-wire hole 的 profile face-with-holes / `InternalShape` bounded-region 分离、on-face open splitter edges、重叠闭合 profile、自相交单 wire、inter-edge intersection split 和基础 face-with-holes / island；FaceMakerBuildFace 的 self-intersection pre-split 与 splitter 阶段已暴露 concrete edge / bounded-face producer evidence，并把 producer context 同步写入 `Sketch.InternalShape` 的 `NamedShape` 元数据；summary 只保留为缺 concrete evidence 时的 diagnostic。
- `geometry/sketch_internal_builder.*` 保留 Pad / Pocket 消费的 `profileShape`，并把 `FaceMakerBuildFace` bounded-region 输出与 WireJoiner open-wire 输出组合为 `internalShape`；open wire 不会伪造成可拉伸 profile。
- `geometry/wire_joiner.*` 已接入 `getOpenWires(noOriginal=true)` 子集：按 FreeCAD `sourceEdgeArray` 口径过滤仍匹配原始 source edge 的 open wire；open edge 可先按 face boundary 切成 EdgeInfo，再逐 EdgeInfo 判断是否被 bounded face 消费，外部非原始 split EdgeInfo 可进入 `InternalShape`；闭合 source wire 循环已从 bounded-face 数量差改为 source edge 被 bounded-result fragments 替换的 ownership 证据触发 copied result-wire graph；cad-core 账本已用 EdgeInfo / WireInfo 承接当前边级 ownership，并按 `iteration == -3 || (!wireInfo && iteration >= 0)` 子集表达 openWireCompound 输出判定。bounded-face ownership 现在写入 tight-bound primary / secondary owner slot；split 后会重建 request-local ordered `WireInfo::vertices` 账本、标记 `iteration2`，并记录端点邻接 branch-search candidate、inside/outside 分类、`newWire` seed 候选、splitWire candidate、done WireInfo、owner propagation 与 `exhaustTightBound()` secondary-owner 诊断账本；S6 已把可恢复 child-wire/current-member vertex evidence 接入 topo mapper events 和唯一 ElementMap alias，不可证明的一对多仍输出 `wire_joiner_current_member_vertex_multiplicity_blocked`。
- `topo/element_map.*` 的几何匹配式基础 internal map 只覆盖未发生复杂 split / merge / deleted 的 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN` simple alias；`topo/named_shape.*` 已在 `Sketch.InternalShape` 上优先消费 FaceMaker / WireJoiner producer evidence，再记录通用 `element_history_status` 和 InternalShape generated / split / deleted history；`InternalFaceN` 已按 FaceMaker 外环边命名口径记录 outer-boundary-`EdgeN` generated history，hole / inner wire 不混入同一个 face 来源，但不写 raw `FaceN` alias；raw sketch edge 被拆成多个 `InternalEdgeN` 时已记录 terminal split history，自交单边 pre-split 也记录为 raw `EdgeN` 到多个 `InternalEdgeN` 的 terminal split history；被 `noOriginal` 过滤且没有 Internal* target 的 raw `EdgeN/VertexN` 已记录 terminal deleted history，并明确不写入单一可解析 `ElementMap` target。

## 已验收能力

- 外矩形 + 中间贯穿线可形成多个 `InternalFaceN`，并要求 `Profile.SubList=InternalFaceN` 才允许 Pad/Pocket 选择局部 profile。
- 外矩形 + 内圆孔、外矩形 + 内圆孔 + island、外矩形 + 内矩形 taper profile 已由 native expected 固定：Sketch `InternalShape` 发布 bounded region face count，Pad 仍按 closed profile face-with-hole / island 得到实体结果。
- BSpline InternalShape 已由 FreeCADCmd oracle 固定 quadratic profile、degree-1 figure-8、cubic figure-8 pre-split 和 overlapping BSpline empty InternalShape；cubic figure-8 已约束 raw `Edge1` 到多个 `InternalEdgeN` 的 terminal split history。
- 十字切割、重叠矩形、重叠圆、自相交 bowtie、cross pattern、dangling open line、split-and-dangling open wires、through open cutter split fragments 已进入 P5 fixture；dangling source line 当前按 FreeCAD `noOriginal=true` 不进入 `InternalShape`。
- Pad 可使用 `InternalFaceN` 拉伸局部 bounded region；纯 open profile 仍通过 `open_profile` 失败，不会静默兜底成 face。
- `fixtures/p5/expected` 已覆盖这些成功几何 fixture 的 internal face count、edge / vertex count 或最低 count、Pad volume 和 SketchPlaneFrame mesh bbox。

## 剩余缺口

- S6 已关闭 recoverable WireJoiner ownership 子集：EdgeInfo / WireInfo、openWireCompound child-wire ledger、current-member vertex evidence 和 `aHistory` 能证明唯一目标时进入 topo mapper events / ElementMap alias；不可证明的一对多保持 stable diagnostic，不做 output pruning。
- S6 已关闭 FaceMaker concrete producer 子集：pre-split / splitter concrete evidence 进入 `Sketch.InternalShape` 的 `NamedShape` 与 topo 消费路径；summary 仍可作为 producer-missing diagnostic，但不再关闭可恢复 case。
- 近切线、重合边、复杂开放线网、非平面/复杂投影，以及 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow` 的持久 stable selector 支持仍需 FreeCAD oracle 或明确协议后再进入主路径。

## FreeCAD 依据

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::Build()` / `FaceMaker::postBuild()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/FaceMakerBuildFace.cpp::FaceMakerBuildFace::Build_Essence()`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoiner::getOpenWires()` / `WireJoinerP::build()`

## cad-core 落点

| 模块 | 状态 |
| --- | --- |
| `features/sketch_object.cpp` | Sketch 执行顺序、`InternalShape` 发布、SubList profile 选择 |
| `geometry/sketch_internal_builder.*` | `FaceMakerBuildFace` bounded result 与 open-wire result 组合 |
| `geometry/face_maker.*` | bounded split / face-with-holes / overlap / self-intersection 子集、FaceMakerBuildFace pre-split / splitter concrete producer evidence 和 summary diagnostic |
| `geometry/wire_joiner.*` | `noOriginal` 原始 source edge 过滤、EdgeInfo / WireInfo 边级账本、ordered `WireInfo::vertices` / `iteration2` 标记、branch-search candidate inside/outside、`newWire` seed、splitWire / done lifecycle 与 `exhaustTightBound()` secondary-owner 诊断账本、bounded tight-bound primary / secondary owner slot 汇总、`wireInfo` / `iteration=-3` openWireCompound 判定子集、open edge split fragment、closed-source result-fragment ownership、child-wire/current-member producer evidence 和 open-wire carry-through 子集 |
| `topo/element_map.*` / `topo/named_shape.*` | simple-alias internal element map、`Sketch.InternalShape` FaceMaker/WireJoiner producer history context、通用 `element_history_status`、`InternalFaceN` outer-boundary generated history、self-intersecting edge pre-split terminal split history、one-source-to-many `InternalEdgeN` split history、one-source-to-zero `EdgeN/VertexN` deleted history和 stable diagnostics |

## 下一步

1. 用 FreeCAD oracle 固定近切线、重合边和复杂 open-wire case，再扩大 P5 fixture。
2. 对不可证明的一对多 open-wire history 继续保持 stable diagnostic，不新增按 source index、split order 或输出排序选择目标的规则。
3. 若要支持 `Profile.StableSubList=InternalFaceN` without `ReferenceShadow`，先补 FreeCAD evidence 和 ElementMap-backed stable selector，再改变 unsupported 状态。

## 验收

- P5 focused tests 与 expected fixture 自动遍历必须同时覆盖 P5b 成功几何。
- `InternalFaceN` 可作为正式 sketch profile token，并可记录 outer-boundary generated history；raw `FaceN` 不得映射成稳定 `InternalFaceN`。
- open wire 不得产生假 `profile_ready=true`。
- split / merge / deleted 的 internal element map 必须来自 history 或明确 diagnostics，不允许继续叠加 fixture 特判。
