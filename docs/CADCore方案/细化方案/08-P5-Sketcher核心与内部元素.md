# P5：Sketcher 核心与内部元素

P5 让 Sketch 不只是 Pad 的简单 profile，而能承载 FreeCAD 风格外部引用、内部元素和 solver-facing 子集。

## 当前基线

- 支持 line、arc of circle、arc of ellipse、circle、ellipse 和基础非周期 BSpline profile；Sketch `Point` 会按 FreeCAD `GeomPoint::toShape()` 输出 raw vertex。
- construction geometry 不参与 profile 构面。
- `Coincident` / `Type=1` 可合并 line endpoint。
- 多闭合 wire 可构成一个基础 face-with-holes；最大面积 wire 作为 outer，奇数层 wire 作为 hole，偶数层嵌套 wire 作为 island face，已覆盖 line outer + circle inner 和 hole-with-island 的混合闭合 profile。该路径只覆盖闭合 wire 分组，不替代完整 FaceMakerBuildFace / WireJoiner 账本。
- open wire sketch 自身成功输出 raw shape，但 `profile_ready=false`，Pad/Pocket 通过 `open_profile` 失败。
- closed sketch 可导出基础 `InternalFaceN` / `InternalEdgeN` / `InternalVertexN`。
- `topo/element_map` 承接最小 `InternalEdgeN/InternalVertexN <-> EdgeN/VertexN` 映射。
- ExternalGeometry 支持 DatumLine / DatumPoint、straight edge、vertex、circle edge、ellipse edge 和 sketch internal edge / vertex 的基础投影。

## 已知缺口

- 完整 constraint solver、BSpline solver/control-point 语义、defining external profile 尚未迁移。
- ExternalGeometry face、非平行 circle/ellipse arc edge 等复杂场景仍未完整。
- `FaceMakerBuildFace` 的 split/intersection、`WireJoiner::getOpenWires()`、复杂 `getInternalElementMap()` 和旧引用恢复不是完整实现。
- open wire 的完整 InternalShape / WireJoiner history 仍需 P5/P6 联合补齐。

## cad-core 落点

| 文件 | 职责 |
| --- | --- |
| `features/sketch_object.*` | SketchObject 执行顺序、profile wire 收集、ExternalGeometry |
| `geometry/face_maker.*` | 多闭合 wire 基础 face-with-holes / island 构面 |
| `topo/element_map.*` | internal element map 基础 |
| `topo/subshape_map.*` | `Internal*` subshape 导出 |
| 后续 `geometry/` | 完整 FaceMakerBuildFace / WireJoiner 正式账本 |

## FreeCAD 依据

- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/Sketcher/App/SketchObjectGeometry.cpp`
- `src/Mod/Sketcher/App/SketchObjectExternal.cpp`
- `src/Mod/Part/App/FaceMaker*.cpp`
- `src/Mod/Part/App/WireJoiner.cpp`

## 验收

- `fixtures/p5` 覆盖 profile、BSpline profile、mixed/nested closed-wire face-with-holes、construction、Coincident、ExternalGeometry、InternalShape 和 unsupported Sketcher 能力；点几何当前由 P7 Hole point fixture 约束。
- open profile 不得伪造成 closed face。
- internal name 解析只在 Sketch `InternalShape` 上生效。
