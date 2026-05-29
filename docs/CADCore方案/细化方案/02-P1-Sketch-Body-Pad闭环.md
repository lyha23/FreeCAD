# P1：Sketch、Body、Pad 闭环

P1 固定第一条真实建模链：`Sketcher::SketchObject -> PartDesign::Pad -> PartDesign::Body -> mesh / bbox / volume / subshape`。

## 当前基线

- Sketch 可从二维几何生成 raw shape，并在闭合 profile 时生成 PartDesign 可用 face。
- Pad 通过 shared `FeatureExtrude` 调用 OCCT prism，输出 add shape。
- Body 按 Group / Tip 组合 feature，输出最终 solid。
- 结果包含 mesh summary、bbox、volume 和 `FaceN` / `EdgeN` / `VertexN` subshape map。

## 语义边界

- Sketch raw shape 和 Pad profile face 是两类结果；open wire sketch 可以成功输出 raw edge，但 Pad/Pocket 必须因 `open_profile` 失败。
- Body 是最终 solid 的组合点；Pad executor 不伪造 Body 结果。
- `Placement`、LinkSub、stable subname、NamedShape 的完整语义由后续阶段扩展，但 P1 输出不能阻塞这些主路径。

## FreeCAD 依据

- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/PartDesign/App/FeaturePad.cpp`
- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/Part/App/TopoShape.cpp`

## 验收

- `fixtures/mvp/rect-pad.json` 输出稳定 bbox、volume、mesh 和 subshape map。
- 非法长度、缺失 profile、unsupported property 返回结构化 diagnostics。
- 后续阶段修改不得破坏 P1 fixture。
