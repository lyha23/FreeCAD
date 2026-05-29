# P3a：FeatureExtrude UpTo 终止语义

P3a 的目标是把 `FeatureExtrude` 从 `Length` only 升级到第一批 FreeCAD 风格终止语义，优先服务 Pad / Pocket 的 `ThroughAll`、`UpToFace`、单目标 `UpToShape`。

## 当前状态

当前工作区已经把 P3a 作为已落地基线处理：

- Pad / Pocket 继续共享 `FeatureExtrude`。
- Pocket 支持 `Type=ThroughAll`。
- Pad / Pocket 支持 `Type=UpToFace`。
- Pocket 支持单目标 `Type=UpToShape`，目标可以是 full solid 或单 face。
- `PropertyLinkSub` 的 `FaceN` 可解析到当前 `ComputeContext` 内目标 shape 的真实 face。
- 缺失目标、非法 subname、非 face subshape、空 UpToShape、多 face UpToShape 都有稳定 diagnostics。
- `fixtures/p3a` 已包含正常 case、错误 case 和 FreeCAD expected。

仍未覆盖：

- `UpToFirst` / `UpToLast`。
- 多 face / shell `UpToShape`。
- `TwoLengths`。
- `SideType=Two sides` / `SideType=Symmetric`。
- taper / taper2。
- custom direction、ReferenceAxis、AlongSketchNormal 完整矩阵。
- attachment / support / object-local placement 的完整恢复。

这些进入 `06-P3b-FeatureExtrude双侧方向与Placement.md` 和后续 P4。

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

FreeCAD 枚举边界：

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

## 实现落点

- `cad-core/src/features/feature_extrude.cpp`：shared builder、method dispatch、UpTo tool shape 构造。
- `cad-core/src/features/pad.cpp`：只保留 Pad 枚举和 additive wrapper。
- `cad-core/src/features/pocket.cpp`：只保留 Pocket 枚举和 subtractive wrapper。
- `cad-core/src/features/body.cpp`：继续由 Body 组合 Fuse / Cut，不让 Pad / Pocket 伪造最终 Body。
- `cad-core/include/cad_core/runtime/compute_context.h`：提供当前 recompute 内 object shape / subshape 查找。
- `cad-core/src/topo/subshape_map.cpp`：提供稳定 `FaceN` / `EdgeN` / `VertexN` 查找基础。
- `cad-core/tests/test_mvp.py`：锁定 P3a 正常与错误 fixture。

## Step 19：冻结 P2 回归面

目标：

- 保留 P2 的 `FeatureBase` / `FeatureAddSub` / Body Fuse/Cut 行为。
- `rect-pad.json` 和 `rect-pad-pocket.json` bbox、volume、topology count 不回退。
- `unsupported-pocket-type.json` diagnostics code 稳定。

验收：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
python3 -m unittest tests/test_mvp.py
```

## Step 20：Pocket ThroughAll

目标：

- 支持 `PartDesign::Pocket` 的 `Type=ThroughAll`。
- 按 FreeCAD `getThroughAllLength()` 语义，用 base solid 和 profile 的 bbox 对角线计算保证穿透长度。
- `ThroughAll` 不要求 `Length`，但仍需要有效 Profile 和前序 base solid。
- 先限制 `SideType=One side`。

fixtures：

```text
fixtures/p3a/
  pocket-through-all.json
  pocket-through-all-without-base.json
  expected/pocket-through-all.freecad.json
```

验收：

- `pocket-through-all.json` diagnostics 为空。
- Cut 后 Body bbox 与 base 一致。
- Cut 后 Body volume / topology count 与 FreeCAD 对照一致。
- 没有 base solid 时返回稳定 diagnostics，不生成假成功 Body。

## Step 21：FaceN subshape 解析

目标：

- 从 `PropertyLinkSub` 读取对象名和 `SubList`。
- 支持 `SubList=["FaceN"]` 解析到目标对象本次 recompute 生成的 face。
- 建立 object result shape 与 subshape map 的查找关系，不依赖 mesh 三角面。
- 对不存在对象、缺失 shape、非法 subname、非 face subname 分别返回稳定 diagnostics。

fixtures：

```text
fixtures/p3a/
  up-to-face-missing-target.json
  up-to-face-missing-subshape.json
  up-to-face-edge-subshape.json
```

验收：

- 缺失目标返回 `missing_link_target`。
- 缺失 subshape 返回 `invalid_subshape`。
- Edge / Vertex 被当作 UpToFace 目标时返回 `unsupported_subshape_kind`。
- 同一输入重复运行时 `FaceN` 解析结果稳定。

## Step 22：UpToFace

目标：

- 支持 `Type=UpToFace`。
- `UpToFace` 必须来自 `PropertyLinkSub`。
- 第一版只支持当前 `ComputeContext` 内 solid 的单个 planar face。
- 根据 sketch profile、方向和目标 face 生成到面为止的 tool shape。
- Pocket 和 Pad 共用同一 builder。

约束：

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
  pad-up-to-face.json
  expected/pocket-up-to-face.freecad.json
  expected/pad-up-to-face.freecad.json
```

验收：

- `pocket-up-to-face.json` diagnostics 为空。
- `pad-up-to-face.json` diagnostics 为空。
- Body volume / bbox / topology count 与 FreeCAD 对照一致。
- 平行 face、与 sketch 相交 face 不生成假成功结果。

## Step 23：UpToShape

目标：

- 支持 `Type=UpToShape`。
- `UpToShape` 来自 `PropertyLinkSubList`。
- 第一版支持单个 full shape 或单个 face。
- 多 face / shell 按 FreeCAD `getUpToShapeFromLinkSubList()` 后置。

fixtures：

```text
fixtures/p3a/
  pocket-up-to-shape-solid.json
  pocket-up-to-shape-face.json
  pocket-up-to-shape-empty.json
  pocket-up-to-shape-multi-face-unsupported.json
  expected/pocket-up-to-shape-solid.freecad.json
  expected/pocket-up-to-shape-face.freecad.json
```

验收：

- 单 solid / 单 face case 与 FreeCAD 对照一致。
- 多 face 暂未支持时返回 `unsupported_property` 或 `unsupported_subshape_kind`。
- 空 `UpToShape` 不静默退回 `Length`。

## P3a 完成定义

P3a 完成需要同时满足：

- P0/P1/P2 全部现有测试继续通过。
- `fixtures/p3a` 正常 case diagnostics 为空。
- `fixtures/p3a` 错误 case diagnostics code 稳定。
- Pocket `ThroughAll`、`UpToFace`、`UpToShape` 有 FreeCAD 对照。
- 至少一个 Pad `UpToFace` 或 `UpToShape` fixture 通过。
- `FeatureExtrude` 中不存在 Pad / Pocket 分叉复制的几何实现。
- 文档同步更新 `00-CAD-Core完整抽取执行总览.md` 和 `03-接口与验收样例.md`。

## 后续入口

P3a 稳定后进入：

```text
06-P3b-FeatureExtrude双侧方向与Placement.md
  -> Two sides / Symmetric
  -> taper / taper2
  -> custom direction / ReferenceAxis / AlongSketchNormal
  -> object-local placement

07-P4-Document-Property-Placement完整化.md
  -> 属性系统、链接系统、坐标系统一
```
