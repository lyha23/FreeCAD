# P3b：FeatureExtrude 双侧、方向与 Placement

P3b 的目标是把 P3a 已有的单侧 UpTo 能力扩成 FreeCAD 风格 `FeatureExtrude` 参数矩阵。完成后 Pad / Pocket 仍共用同一套 shared builder，后续 Pattern / Mirror / Hole / DressUp 才能复制或引用可靠的 source feature。

## 当前输入

P3a 已具备：

- `Type=Length`。
- Pocket `Type=ThroughAll`。
- Pad / Pocket `Type=UpToFace`。
- Pocket 单目标 `Type=UpToShape`。
- `PropertyLinkSub FaceN` 到当前 recompute 内目标 face 的解析。

P3b 在这个基础上补：

- `SideType=Two sides`。
- `SideType=Symmetric`。
- `Type2` / `Length2` / `UpToFace2` / `UpToShape2`。
- 单侧 `UpToFirst` / `UpToLast` 从 previous body solid 中选择最近 / 最远候选面。
- taper / taper2。
- custom direction / ReferenceAxis / AlongSketchNormal。
- object-local placement 和 sketch normal 的一致性。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| shared extrusion 入口 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`：`FeatureExtrude::buildExtrusion()` |
| 单侧生成 | `FeatureExtrude.cpp`：`generateSingleExtrusionSide()` |
| 双侧 / 对称组合 | `FeatureExtrude.cpp` 中 SideType、Type2、Length2、TaperAngle2 相关分支 |
| 方向计算 | `FeatureExtrude.cpp`：`computeDirection()` |
| profile / support | `src/Mod/PartDesign/App/FeatureSketchBased.cpp` |
| UpToFirst / UpToLast face selection | `src/Mod/Part/App/PartFeature.cpp`：`findAllFacesCutBy()`；`src/Mod/PartDesign/App/FeatureSketchBased.cpp`：`ProfileBased::getUpToFace()` |
| Placement | `src/App/GeoFeature.cpp`、`src/App/GeoFeatureGroupExtension.cpp` |

## 实现落点

- `features/feature_extrude.*`：SideType matrix、方向计算、双侧 tool shape 合成。
- `features/pad.*` / `features/pocket.*`：只维护各自 TypeEnums 和 additive / subtractive wrapper。
- `features/feature_base.*`：base solid、body-local placement 和 support 输入。
- `geometry/`：taper prism、方向向量、坐标变换的 OCCT helper。
- `topo/`：双侧、taper、boolean 后的命名传播账本，不能靠导出层补名字。

## 当前落地状态

已进入 `cad-core` 主路径：

- `features/feature_extrude.cpp` 已统一分派 `SideType=One side / Two sides / Symmetric`，Pad / Pocket 仍共用 `buildFeatureExtrusion()`。
- `Two sides` 已支持 `Type=Length` + `Type2=Length` 的不等长组合；第一侧 `Type=UpToFace` / `Type=UpToShape` 与第二侧 `Type2=UpToFace` / `Type2=UpToShape` 都已接入目标解析，缺失第二侧目标会返回具体 diagnostics。
- 单侧 `Type=UpToFirst` / `Type=UpToLast` 已按 FreeCAD `findAllFacesCutBy()` 基线接入：从上一 solid 的被切候选面中按 `distsq` 选择最近 / 最远面，再复用当前平面 UpTo 测量路径生成 tool shape。
- `Symmetric` 已支持 `Type=Length`；非 taper length 路径按 FreeCAD 把 profile 平移到中面一侧再拉伸完整长度，taper 路径按 FreeCAD 把长度减半后沿正反两个方向分别生成 drafted prism 再 fuse。
- `UseCustomVector + Direction` 已支持显式三维向量；`AlongSketchNormal=true` 时按方向与 sketch normal 的点积校正长度。零向量或正交方向返回 `invalid_direction`。
- `ReferenceAxis` 已支持 `App::PropertyLinkSub` 到 profile sketch 轴的基础路径：`N_Axis` 可作为拉伸方向，`H_Axis` / `V_Axis` 会按 FreeCAD `NotPerpendicularWithNormal` 规则返回 `invalid_direction`。实现中也接入了已计算 shape 的 `EdgeN` 直线 / 圆弧轴解析，以及 `PartDesign::Line` DatumLine 目标的直接方向解析；坏链接返回 `missing_link_target`。
- `geometry/placement.*` 已提供共享 `App::PropertyPlacement` 解析，按 `Base` + quaternion `Rotation` 生成 `gp_Trsf`，并复用到 Sketch、Body、FeatureBase。
- `Sketcher::SketchObject` 已读取 `App::PropertyPlacement`，先把 profile face 变换到当前请求坐标系，再交给 shared extrusion 使用；P3b 已覆盖纯平移和 90 度旋转后 sketch normal 参与 `AlongSketchNormal` 长度修正。
- `Body` 已在最终 body shape 输出前应用自身 Placement，Pocket cut 后的 bbox / mesh / subshape map 与被放置后的 body 坐标一致。
- `FeatureBase` 已对导入 base solid 应用自身 Placement，使 Body 采用已经位于目标坐标的 base feature。
- `geometry/extrusion_helper.*` 已迁入 FreeCAD `Part::ExtrusionHelper::makeElementDraft()` 的主几何流程：端面 wire offset、`BRepOffsetAPI_ThruSections` ruled loft、face 内 wire cut；`FeatureExtrude` 已用它支持 Pad / Pocket 的 `TaperAngle`，`Two sides` 下独立的 `TaperAngle` / `TaperAngle2`，以及 `Symmetric` 下的单侧 taper。
- P3b/P4 fixture 已覆盖双侧长度、第一侧 `UpToFace` / `UpToShape`、第二侧 `UpToFace2` / `UpToShape2`、单侧 `UpToFirst` / `UpToLast` previous-body 最近 / 最远面、Pocket 双侧 cut、对称长度、对称 taper、Pad / Pocket custom vector、ReferenceAxis sketch `N_Axis`、ReferenceAxis EdgeN、ReferenceAxis DatumLine、ReferenceAxis parallel-axis / missing-target error、sketch placement、Body placement、FeatureBase placement、custom direction + sketch translation / rotation placement、Pad taper、Pocket taper、two-sides taper、invalid taper、symmetric UpTo 暂缓和第二侧目标缺失。
- `fixtures/p3b/expected` 已补 21 个 FreeCAD oracle 快照；`tests/test_mvp.py` 对 P3b 成功 fixture 读取 expected，并校验 bbox、volume 和 topology counts。
- C ABI harness 已用 P3b `pocket-custom-vector` 和 CLI 输出做同输入一致性校验，覆盖 diagnostics、objects、mesh、subshapes。

仍是明确暂缓边界：

- taper 的几何结果已经生成，但 `TopoShape::makeElementShape(...)` 对应的 Modified / Generated history 还没有进入 `topo` 账本；taper fixture 当前显式标记 `topo_naming=known_gap:taper_history`，归入 P6 继续补。
- `UpToFirst` / `UpToLast` 当前只覆盖 line-through-profile-center 命中的平面候选面；完整 `makeElementPrismUntil()` 的非平面终止面、多 wire support face 和 offset shell 仍是后续通用化边界。
- `Symmetric + UpTo*` 的 mirror 路径还未迁移，现在返回 `unsupported_property`，避免生成假成功。
- `ReferenceAxis` 的 P3b 子集已落地并开始消费 P4 LinkSub 结构；DatumLine 目标已可直接取方向，外部 Part feature edge ownership 与旧引用恢复仍未完成。
- 完整 object-local inverse placement、GeoFeatureGroup 层级 placement、旧 LinkSub 恢复和 placement 变更后的拓扑命名恢复仍归入 P4 / P6；当前 P3b 只保证显式 Sketch / Body / FeatureBase Placement 不靠输出端修正。

当前验收命令：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

当前验证结果：`63 tests OK`，P0/P1/P2/P3a fixture 未回退。

## Step 26：SideType dispatcher

目标：

- 在 shared `FeatureExtrude` 中统一处理 `One side`、`Two sides`、`Symmetric`。
- 旧 `Length`、`ThroughAll`、`UpToFace`、`UpToShape` 路径保持不回退。
- Pad / Pocket 不复制 SideType 分支。

验收：

- P0/P1/P2/P3a fixture 继续通过。
- 未支持组合返回 `unsupported_property`，不静默退回 `One side`。

## Step 27：Two sides

目标：

- 支持第一侧 `Type` / `Length` / `UpToFace` / `UpToShape`。
- 支持第二侧 `Type2` / `Length2` / `UpToFace2` / `UpToShape2`。
- 第一版可以先限定两侧都是 planar target 或 length。
- 两侧 tool shape 合成后仍交给 Body 做 Fuse / Cut。

fixtures：

```text
fixtures/p3b/
  pad-two-sides-length.json
  pad-two-sides-up-to-face1.json
  pad-two-sides-up-to-shape1.json
  pad-two-sides-up-to-face2.json
  pad-two-sides-up-to-shape2.json
  pocket-two-sides-length.json
```

验收：

- 两侧长度不等时 bbox / volume 与 FreeCAD oracle 一致。
- 第二侧缺失目标返回具体 diagnostics。
- Body 仍负责最终 add / subtract。

## Step 27a：UpToFirst / UpToLast

目标：

- 对齐 FreeCAD `ProfileBased::getUpToFace()` 中 `UpToFirst` / `UpToLast` 的候选面选择：从 previous body solid 中用 `findAllFacesCutBy()` 取沿拉伸方向被草图重心线切到的 face。
- `UpToFirst` 选择最近候选面，`UpToLast` 选择最远候选面。
- 第一版限定当前已支持的平面终止面；非平面 `makeElementPrismUntil()`、offset 和多 face shell 归入后续边界。

fixtures：

```text
fixtures/p3b/
  pad-up-to-first.json
  pad-up-to-last.json
```

验收：

- 同一 previous body solid 下，`UpToFirst` / `UpToLast` 生成不同长度的 Pad tool shape。
- diagnostics 为空；bbox / volume 稳定约束最近 / 最远面选择。
- 不通过 fixture 名称或输出端修剪决定长度。

## Step 28：Symmetric

目标：

- 支持 `SideType=Symmetric`。
- `Length` 对称分配到 profile 两侧。
- UpTo 组合若 FreeCAD 行为复杂，可先诊断暂缓，不允许生成假成功。

fixtures：

```text
fixtures/p3b/
  pad-symmetric-length.json
  pocket-symmetric-length.json
  pad-symmetric-up-to-unsupported.json
```

验收：

- symmetric length 的 bbox / volume 与 FreeCAD oracle 一致。
- unsupported symmetric UpTo 返回稳定 diagnostics。

## Step 29：Taper / Taper2

目标：

- 支持 Pad / Pocket 的 taper angle。
- `Two sides` 时区分 `TaperAngle` 和 `TaperAngle2`。
- taper 结果的 face / edge source trace 要进入 topo 账本，不能只改最终 shape。

fixtures：

```text
fixtures/p3b/
  pad-length-taper.json
  pocket-length-taper.json
  pad-two-sides-taper.json
  pad-symmetric-taper.json
  pocket-invalid-taper.json
```

验收：

- taper 后 bbox / volume / topology count 与 FreeCAD oracle 一致。
- 无效 taper 或自相交返回 diagnostics。
- topo 命名缺口若暂未完成，必须在 fixture 分类中标为 known topo gap，而不是当成功 parity。

## Step 30：Custom direction

目标：

- 支持 `UseCustomVector`、`Direction`、`ReferenceAxis`、`AlongSketchNormal`。
- 方向向量进入 body-local / object-local placement 计算。
- ReferenceAxis LinkSub 解析使用 P4 的统一 LinkSub 结构；如果 P4 未完成，P3b 只能先支持显式向量。

fixtures：

```text
fixtures/p3b/
  pad-custom-vector.json
  pocket-custom-vector.json
  pad-reference-axis.json
  pad-reference-axis-edge.json
  ../p4/datum-line-reference-axis.json
  pad-invalid-direction.json
  pad-reference-axis-missing-target.json
```

验收：

- 显式向量方向下 bbox / volume 与 FreeCAD oracle 一致。
- 零向量、缺失 reference axis 返回 diagnostics。

## Step 31：Placement 对齐

目标：

- Sketch placement、Body placement、FeatureBase placement 在拉伸方向和结果 shape 中一致生效。
- 不在 adapter 或 fixture expected 中补坐标偏移。

fixtures：

```text
fixtures/p3b/
  pad-sketch-placement.json
  pocket-body-placement.json
  body-basefeature-placement.json
  pad-custom-direction-placement.json
  pad-custom-direction-sketch-rotation.json
```

验收：

- bbox、subshape map 和 picked subname 都位于同一坐标语义下。
- placement 修改后旧 LinkSub 不应静默指向错误几何；若 topo naming 尚未恢复，必须 diagnostics 或标为 P6 gap。

## 完成定义

P3b 完成需要同时满足：

- P0/P1/P2/P3a fixtures 不回退。
- Pad / Pocket 仍共享 `FeatureExtrude`。
- `Two sides`、`Symmetric`、taper、custom direction 至少各有成功和错误 fixture。
- FreeCAD oracle 覆盖每类成功 fixture。
- placement 不靠输出端修正。
- topo naming 不完整的地方有明确 P6 跟踪项，不能作为隐式成功。
