# P3a：FeatureExtrude UpTo 终止语义

P3a 的目标是把 `FeatureExtrude` 从 `Length` only 升级到 FreeCAD 风格的终止语义。优先服务 Pad / Pocket，尤其是 Pocket 的 `ThroughAll`、`UpToFace`、`UpToShape`。

## 当前边界

当前实现位置：

- `cad-core/src/features/feature_extrude.cpp`
- `cad-core/src/features/pad.cpp`
- `cad-core/src/features/pocket.cpp`
- `cad-core/src/features/body.cpp`
- `cad-core/src/document/model.cpp`

当前能力：

- `FeatureExtrude` 只支持 `Type=Length`。
- `SideType` 只支持 `One side`。
- Pad / Pocket 通过 `AddSubMode` 区分 additive / subtractive。
- Pocket 依赖显式 `Reversed` 调整当前简化坐标模型下的减料方向。
- `PropertyLinkSub` 已能形成 dependency edge，但还没有把 `FaceN` 解析成可用于 UpTo 的真实 face。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| 共享执行入口 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`：`FeatureExtrude::buildExtrusion()` |
| 单侧生成 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`：`FeatureExtrude::generateSingleExtrusionSide()` |
| 方向计算 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`：`FeatureExtrude::computeDirection()` |
| ThroughAll 长度 | `src/Mod/PartDesign/App/FeatureSketchBased.cpp`：`ProfileBased::getThroughAllLength()` |
| UpToFace 解析 | `src/Mod/PartDesign/App/FeatureSketchBased.cpp`：`ProfileBased::getUpToFaceFromLinkSub()`、`getUpToFace()` |
| UpToShape 解析 | `src/Mod/PartDesign/App/FeatureSketchBased.cpp`：`ProfileBased::getUpToShapeFromLinkSubList()` |
| Pocket 类型枚举 | `src/Mod/PartDesign/App/FeaturePocket.cpp`：`Pocket::TypeEnums` |
| Pad 类型枚举 | `src/Mod/PartDesign/App/FeaturePad.cpp`：`Pad::TypeEnums` |

FreeCAD 当前枚举边界：

```text
Pocket Type:
  Length
  ThroughAll
  UpToFirst
  UpToFace
  ?TwoLengths
  UpToShape

Pad Type:
  Length
  UpToLast
  UpToFirst
  UpToFace
  ?TwoLengths
  UpToShape

SideType:
  One side
  Two sides
  Symmetric
```

## Step 19：冻结 P2 回归面

做什么：

- 保留 P2 的 `FeatureBase` / `FeatureAddSub` / Body Fuse/Cut 行为。
- 在改 `FeatureExtrude` 前先跑全量现有测试。
- 给 `FeatureExtrude` 加 method 分发，但默认路径仍走 `Length`。

验收：

- `python3 -m unittest discover -s cad-core/tests` 仍通过。
- `rect-pad.json` 和 `rect-pad-pocket.json` bbox、volume、topology count 不变。
- `unsupported-pocket-type.json` 在新 method 未实现前仍返回稳定 diagnostics。

## Step 20：实现 Pocket ThroughAll

做什么：

- 支持 `PartDesign::Pocket` 的 `Type=ThroughAll`。
- 按 FreeCAD `getThroughAllLength()` 语义，用 base solid 和 profile 的 bbox 对角线计算保证穿透长度。
- `ThroughAll` 不要求 `Length`，但仍需要有效 Profile 和前序 base solid。
- 先限制 `SideType=One side`。

实现落点：

- `feature_extrude.cpp`：把 `buildLengthExtrusion()` 拆成 method-aware 的 shared builder。
- `body.cpp`：保持 subtractive shape 由 Body Cut，不把 ThroughAll 直接写成最终 Body。
- `tests/test_mvp.py`：新增 P3a diagnostics 和结果断言。

fixtures：

```text
fixtures/p3a/
  pocket-through-all.json
  pocket-through-all-without-base.json
  expected/
    pocket-through-all.freecad.json
```

验收：

- `pocket-through-all.json` diagnostics 为空。
- Cut 后 Body bbox 与 base 一致。
- Cut 后 Body volume 与 FreeCAD 对照一致。
- 没有 base solid 时返回 `execution_failed` 或更具体 diagnostics，不生成假成功 Body。

## Step 21：实现 FaceN subshape 解析

做什么：

- 从 `PropertyLinkSub` 读取对象名和 `SubList`。
- 支持 `SubList=["FaceN"]` 解析到目标对象本次 recompute 生成的 face。
- 建立 object result shape 与 subshape map 的双向查找，不依赖 mesh 三角面。
- 对不存在对象、缺失 shape、非法 subname、非 face subname 分别返回稳定 diagnostics。

建议 diagnostics：

```text
missing_link_target
invalid_subshape
unsupported_subshape_kind
```

如果暂时不新增 code，可以先用 `missing_link_target` / `unsupported_property`，但最终应拆出更精确 code，方便 fixture 锁定。

验收：

- `up-to-face-missing-target.json` 返回稳定 diagnostics。
- `up-to-face-missing-subshape.json` 返回稳定 diagnostics。
- `up-to-face-edge-subshape.json` 不把 Edge 当 Face 使用。
- 同一输入重复运行时 `FaceN` 解析结果稳定。

## Step 22：实现 UpToFace

做什么：

- 支持 `Type=UpToFace`。
- `UpToFace` 必须是 `PropertyLinkSub`。
- 第一版只支持目标为当前 `ComputeContext` 内 solid 的单个 planar face。
- 根据 sketch profile、方向和目标 face 生成到面为止的 tool shape。
- Pocket 先落地；Pad 可复用同一 builder 后再打开 fixture。

实现约束：

- 不在 Pocket 中直接特殊处理 UpToFace。
- 不通过 mesh 近似裁剪。
- 不能只用目标对象 bbox 替代目标 face。
- UpToFace 与 sketch face 相交或方向平行时必须诊断失败。

fixtures：

```text
fixtures/p3a/
  pocket-up-to-face.json
  pocket-up-to-face-parallel.json
  pocket-up-to-face-intersects-sketch.json
  expected/
    pocket-up-to-face.freecad.json
```

验收：

- `pocket-up-to-face.json` diagnostics 为空。
- Body volume / bbox / topology count 与 FreeCAD 对照一致。
- 平行 face、与 sketch 相交 face 不生成假成功结果。

## Step 23：实现 UpToShape

做什么：

- 支持 `Type=UpToShape`。
- `UpToShape` 必须是 `PropertyLinkSubList`。
- 第一版先支持单个 full shape 或单个 face。
- 多 face / shell 按 FreeCAD `getUpToShapeFromLinkSubList()` 语义后置。

fixtures：

```text
fixtures/p3a/
  pocket-up-to-shape-solid.json
  pocket-up-to-shape-face.json
  pocket-up-to-shape-multi-face-unsupported.json
  expected/
    pocket-up-to-shape-solid.freecad.json
    pocket-up-to-shape-face.freecad.json
```

验收：

- 单 solid / 单 face case 与 FreeCAD 对照一致。
- 多 face 暂未支持时返回 `unsupported_property` 或 `unsupported_subshape_kind`。
- 空 `UpToShape` 不应静默退回 Length。

## Step 24：补 Pad UpTo 覆盖

做什么：

- 在 Pocket 的 ThroughAll / UpToFace / UpToShape 稳定后，打开 Pad 的 UpTo 路径。
- Pad 不支持 `ThroughAll`；不要把 Pocket 的 TypeEnums 直接复用给 Pad。
- 先做 `UpToFace` / `UpToShape`，再做 `UpToFirst` / `UpToLast`。

fixtures：

```text
fixtures/p3a/
  pad-up-to-face.json
  pad-up-to-shape-face.json
  pad-up-to-first.json
  pad-up-to-last.json
```

验收：

- Pad UpTo 不破坏 P1 `rect-pad.json`。
- Pad / Pocket 仍共用 `FeatureExtrude`。
- Pad 的 unsupported Type 不能因为 Pocket 扩展而误通过。

## Step 25：暂缓 Two sides / Symmetric

`SideType=Two sides` 和 `SideType=Symmetric` 暂缓到 P3b。原因是 FreeCAD 对这些情况会组合 `Type` / `Type2`、`Length` / `Length2`、`UpToFace2` / `UpToShape2`、双侧 taper 和镜像逻辑；如果和 UpTo 一起做，会扩大验证面。

P3a 只要求：

```text
SideType = One side
Type = ThroughAll | UpToFace | UpToShape
Pad/Pocket 仍共享 FeatureExtrude
```

## 完成定义

P3a 完成需要同时满足：

- P0/P1/P2 全部现有测试继续通过。
- `fixtures/p3a` 正常 case diagnostics 为空。
- `fixtures/p3a` 错误 case diagnostics code 稳定。
- Pocket `ThroughAll`、`UpToFace`、`UpToShape` 有 FreeCAD 对照。
- 至少一个 Pad `UpToFace` 或 `UpToShape` fixture 通过。
- `FeatureExtrude` 中不存在 Pad/Pocket 分叉复制的几何实现。
- 文档同步更新 `00-MVP执行总览.md` 和 `03-接口与验收样例.md` 的后续路线。

## P3a 之后

P3a 稳定后再进入：

```text
P3b SideType matrix
  -> Two sides / Symmetric
  -> taper / taper2
  -> custom direction / ReferenceAxis / AlongSketchNormal
  -> object-local placement

P4 Transformed family
  -> FeatureTransformed base semantics
  -> LinearPattern
  -> Mirrored
  -> PolarPattern / MultiTransform

P5 DressUp / Hole
  -> Hole
  -> Fillet / Chamfer
  -> Refine
```
