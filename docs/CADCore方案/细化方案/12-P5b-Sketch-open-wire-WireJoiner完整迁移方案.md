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
- `geometry/face_maker.*` 已覆盖 bounded split 子集：closed profile wires、on-face open splitter edges、重叠闭合 profile、自相交单 wire、inter-edge intersection split 和基础 face-with-holes / island。
- `geometry/sketch_internal_builder.*` 已把 bounded faces 作为 `profileShape`，并把 WireJoiner open-wire 输出追加到 `internalShape`；Pad / Pocket 只消费 face 子集，不把 open wire 伪造成可拉伸 profile。
- `geometry/wire_joiner.*` 已接入 `getOpenWires(noOriginal=true)` 子集：按 FreeCAD `sourceEdgeArray` 口径过滤仍匹配原始 source edge 的 open wire；open edge 可先按 face boundary 切成 fragments，再逐 fragment 判断是否被 bounded face 消费，外部非原始 split fragments 可进入 `InternalShape`。
- `topo/element_map.*` 仍是几何匹配式基础 internal map，只覆盖未发生复杂 split / merge / deleted 的 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN`。

## 已验收能力

- 外矩形 + 中间贯穿线可形成多个 `InternalFaceN`，并要求 `Profile.SubList=InternalFaceN` 才允许 Pad/Pocket 选择局部 profile。
- 十字切割、重叠矩形、重叠圆、自相交 bowtie、cross pattern、dangling open line、split-and-dangling open wires、through open cutter split fragments 已进入 P5 fixture；dangling source line 当前按 FreeCAD `noOriginal=true` 不进入 `InternalShape`。
- Pad 可使用 `InternalFaceN` 拉伸局部 bounded region；纯 open profile 仍通过 `open_profile` 失败，不会静默兜底成 face。
- `fixtures/p5/expected` 已覆盖这些成功几何 fixture 的 internal face count、edge / vertex count 或最低 count、Pad volume 和 SketchPlaneFrame mesh bbox。

## 剩余缺口

- 当前 `WireJoiner` 仍是临时 ownership 子集：用 bounded-face count 变化判断 open wire 是否被 bounded face 消费；还没有完整迁移 FreeCAD `WireJoinerP::EdgeInfo`、`WireInfo`、`wireInfo/wireInfo2`、`iteration`、`superEdge` 和 `openWireCompound` 生命周期。
- `FaceMaker::postBuild()`、pre-split history、splitter history、source shape 映射还没有形成完整 MapperHistory；复杂 split / merge / deleted 不能靠当前几何匹配式 `internal_element_map` 证明稳定引用。
- BSpline InternalShape 的 dedicated FreeCAD oracle 尚未冻结，相关 expected 仍以 `known_gap: internal_shape_oracle_pending` 标注。
- 近切线、重合边、复杂开放线网和非平面/复杂投影场景仍需 FreeCAD oracle 后再进入主路径。

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
| `geometry/face_maker.*` | bounded split / face-with-holes / overlap / self-intersection 子集 |
| `geometry/wire_joiner.*` | `noOriginal` 原始 source edge 过滤、open edge split fragment 和 open-wire carry-through 临时子集；后续替换为 WireJoinerP ownership |
| `topo/element_map.*` | 基础 internal element map；后续消费 FaceMaker / WireJoiner history |

## 下一步

1. 迁移 `WireJoinerP::EdgeInfo/WireInfo` 状态机，让 open-wire ownership 不再依赖 bounded-face count 差值。
2. 把 `FaceMaker::postBuild()`、pre-split history 和 splitter history 接入 P6 `NamedShape` / `ElementMap`。
3. 用 FreeCAD oracle 固定 BSpline、近切线、重合边和复杂 open-wire case，再扩大 P5 fixture。

## 验收

- P5 focused tests 与 expected fixture 自动遍历必须同时覆盖 P5b 成功几何。
- `InternalFaceN` 可作为正式 sketch profile token；`InternalEdgeN/InternalVertexN` 只能由 `InternalShape` 导出。
- open wire 不得产生假 `profile_ready=true`。
- split / merge / deleted 的 internal element map 必须来自 history 或明确 diagnostics，不允许继续叠加 fixture 特判。
