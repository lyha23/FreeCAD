# ReferenceShadow 旧几何快照恢复方案

## 结论

`ReferenceShadow` 是未来实现方案，不是 FreeCAD 原样字段。它的设计依据是 FreeCAD 的
`ShadowSub + ElementCache + findSubShapesWithSharedVertex()` 机制，但落到当前 CAD Core
前后端架构时，需要把 FreeCAD 的运行期内存旧 subshape cache 转换成随请求传递的引用恢复证据。

核心原则：

- `DocumentObject graph` 仍然是唯一真实建模数据。
- `ReferenceShadow` 只用于引用解析、漂移检测和引用更新建议，不参与 shape 构造。
- 旧几何 snapshot 是恢复证据，不是前端长期几何状态，也不是后端 session cache。
- `ReferenceShadow.brep` 已作为接口例外批准；它只允许保存被引用单个旧 subshape 的 BREP snapshot，不能保存完整对象 BREP。
- 只在唯一高置信候选时自动更新；多候选、split、deleted 或低置信场景返回诊断，让用户重选。

## FreeCAD 依据

### FreeCAD 确认存在的机制

- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/ElementNamingUtils.h::ElementNamePair`
  定义 `newName` / `oldName`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.h::PropertyLinkBase::ShadowSub`
  是 `ElementNamePair`。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp::PropertyLinkBase::_updateElementReference()`
  会先解析 `shadow.newName` / `shadow.oldName`；当引用缺失或反向更新时，调用
  `GeoFeature::searchElementCache()` 用旧几何搜索新元素名。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/App/GeoFeature.h::searchElementCache()`
  注释说明：几何属性改变前 snapshot 所有被引用元素几何，改变后用旧几何搜索新元素名。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::ElementCache`
  保存旧 `TopoShape shape`、候选 `names` 和 `searched` 状态。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::onBeforeChange()`
  通过 `propShape->getShape().getSubTopoShape(...)` 缓存被引用旧 subshape。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PartFeature.cpp::Feature::searchElementCache()`
  在当前新 shape 上调用 `findSubShapesWithSharedVertex()` 查找候选名。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::findSubShapesWithSharedVertex()`
  按共享顶点、ancestor 结构和几何一致性搜索 Vertex / Edge / Face / composite shape。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject()`
  调用 `registerElementCache(internalPrefix(), &InternalShape)`，说明 Sketch `InternalShape`
  参与 FreeCAD 的旧几何缓存机制。
- `/Users/admin/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/PropertyTopoShape.cpp::PropertyShapeCache`
  的 `Save()` / `Restore()` 为空，动态属性是
  `Prop_NoPersist | Prop_Output | Prop_Hidden`，说明 shape cache 不是持久模型数据。

### 与 FreeCAD 原实现的差异

- FreeCAD 没有名为 `ReferenceShadow` 的持久 JSON 字段。
- FreeCAD 不把旧子元素 BREP 持久保存在文档模型或请求 payload 中；CAD Core 这里是经批准的无状态接口适配。
- FreeCAD 的几何搜索主要在引用缺失、首次生成 element map、版本变化反向更新等路径触发；
  方案中的“解析成功后仍用 fingerprint 检查语义漂移”是 CAD Core 的增强策略。
- 因为当前 CAD Core 后端是无状态计算服务，请求结束后不能持有 FreeCAD 式内存 cache；
  所以 `ReferenceShadow` 是架构适配，不应写成 FreeCAD parity 原字段。

## 问题背景

`StableSubList` 可以表达稳定名，但不能单独证明语义未漂移。典型情况：

```text
旧草图：
底边被 1 条 cutter 切成 2 段
g底边:split1 = 左段
g底边:split2 = 右段

新草图：
新增 1 条 cutter，底边被切成 3 段
g底边:split1 = 左段
g底边:split2 = 中段
g底边:split3 = 右段
```

旧引用 `g底边:split2` 仍可能解析成功，但它已经从“右段/右侧区域”漂移成“中段/中间区域”。
如果 Pad / Pocket 继续静默使用这个解析结果，就会拉伸错误区域。

## 数据模型

### LinkSub 扩展

`ReferenceShadow` 与 `SubList` / `StableSubList` 按下标对齐，只保存被引用元素的恢复证据。该字段已经纳入 `/cad/recompute` 接口，允许随 `PropertyLinkSub` 或 `PropertyLinkSubList.SubSet[]` 出现在请求 `Objects[]` 中，也允许随 `elementReferenceUpdates[]` 返回给前端写回。

```json
{
  "PropertyType": "App::PropertyLinkSub",
  "value": "Sketch",
  "SubList": ["InternalFace2"],
  "StableSubList": ["g305:split2;..."],
  "ShadowSub": [
    {
      "newName": "g305:split2;...",
      "oldName": "InternalFace2"
    }
  ],
  "ReferenceShadow": [
    {
      "target": "Sketch",
      "targetId": 30,
      "property": "InternalShape",
      "shapeType": "Face",
      "indexed": "Face2",
      "subname": "InternalFace2",
      "stableSubname": "g305:split2;...",
      "fingerprint": {
        "area": 1200.0,
        "centroid": [30.0, 10.0, 0.0],
        "normal": [0.0, 0.0, 1.0],
        "bboxMin": [20.0, 0.0, 0.0],
        "bboxMax": [40.0, 20.0, 0.0],
        "edgeCount": 4,
        "vertexCount": 4,
        "boundaryStableSubnames": ["g301", "g302", "g305:split2", "g306:split2"],
        "boundaryVertexPoints": [
          [20.0, 0.0, 0.0],
          [40.0, 0.0, 0.0],
          [40.0, 20.0, 0.0],
          [20.0, 20.0, 0.0]
        ]
      },
      "brep": {
        "format": "brep-bin-zstd-base64",
        "byteLength": 812,
        "sha256": "base16-or-base64-digest",
        "data": "..."
      }
    }
  ]
}
```

字段语义：

| 字段 | 语义 |
| --- | --- |
| `target` | 被引用对象名，例如 `Sketch`。 |
| `targetId` | 被引用对象的稳定 `Objects[].ID`，防止同名对象删除重建后误用旧 snapshot。 |
| `property` | 被引用几何属性；草图内部面使用 `InternalShape`。 |
| `shapeType` | `Vertex` / `Edge` / `Face`。 |
| `indexed` | 捕获时的 indexed name，例如 `Face2`。 |
| `subname` | 捕获时的显示 subname，例如 `InternalFace2`。 |
| `stableSubname` | 捕获时用于长期引用的 stable subname。 |
| `fingerprint` | 轻量几何指纹，用于漂移检测和无 BREP 恢复。 |
| `brep` | 可选旧 subshape 快照，用于贴近 FreeCAD 的旧几何搜索。 |

`brep` 只允许保存被引用子元素，不保存整个对象 BREP。

关键约束：

- `ReferenceShadow` 是可丢弃、可重建的引用恢复证据。字段缺失、`targetId`
  不匹配、BREP 解码失败或 digest 不匹配时，后端应忽略该 shadow 并返回引用诊断，不应用它强行恢复。
- 用户主动重新选择 subshape 时，前端必须清掉旧 `ReferenceShadow`，并按新选择重新捕获；
  不得把旧 face 的 BREP 继续挂在新选择上。
- `ReferenceShadow` 不能替代 `NamedShape` / `ElementMap`。稳定引用主路径仍然是
  `StableSubList -> NamedShape / ElementMap`，旧 BREP 只作为 fallback 证据。
- BREP 和 fingerprint 必须使用同一坐标系：被引用属性 shape 的本地坐标，避免对象 placement 改变后把同一几何误判为漂移。

## 解析流程

### 1. 常规稳定名解析

先按当前已有路径解析：

```text
Profile.SubList / StableSubList
  -> NamedShape / ElementMap
  -> 当前 InternalFaceN / InternalEdgeN / InternalVertexN
```

解析失败时，进入 `ReferenceShadow` 恢复。

### 2. 解析成功后的漂移检查

如果请求携带 `ReferenceShadow`，对当前解析结果计算 fingerprint，并与旧 fingerprint 比较。

Face 建议规则：

- `shapeType` 必须一致。
- 法向夹角小于角度容差。
- 面积差在相对容差内。
- bbox 与 centroid 距离在模型尺度容差内。
- edgeCount / vertexCount 不应无故变化。
- `boundaryStableSubnames` 应有足够交集。

Edge 建议规则：

- 两端点位置接近。
- 直线先比端点。
- 非直线再比曲线几何等价。
- 曲线长度和 bbox 作为辅助校验。

Vertex 建议规则：

- 点位距离在容差内。

校验通过：使用当前解析结果，并在 response 中返回更新后的 `ReferenceShadow`。

校验失败：不要静默使用当前 subname；进入旧几何恢复，或返回 `subname_semantic_drift`。

### 3. 旧几何恢复

如果 shadow 携带 `brep`，解码旧 subshape 后在当前 shape 中搜索同类型候选：

```text
ReferenceShadow.brep
  -> old TopoDS_Shape
  -> current NamedShape
  -> findSubShapesWithSharedVertex-like matcher
  -> Unique / Missing / Ambiguous
```

匹配器应对齐 FreeCAD `TopoShape::findSubShapesWithSharedVertex()`：

- Vertex：按 `BRep_Tool::Pnt()` 和 tolerance 匹配。
- Edge：先用端点找候选 ancestor edge；直线按端点，非直线比较曲线几何等价。
- Face：先用旧 face 外轮廓顶点找候选 face；候选 face 的顶点数、边数、平面、外轮廓边都要匹配。
- Composite shape：递归比较子元素结构。

如果没有 BREP，允许用 fingerprint 做降级恢复，但只能接受唯一高置信候选。

### 4. 更新与诊断

唯一恢复成功：

```text
更新 SubList / StableSubList / ShadowSub / ReferenceShadow
返回 elementReferenceUpdates，其中包含更新后的 ReferenceShadow
```

恢复失败：

- `subname_resolve_failed`：稳定名和旧几何都无法恢复到唯一候选。
- `subname_resolve_ambiguous`：旧几何或 fingerprint 找到多个候选。
- `subname_semantic_drift`：稳定名解析成功，但旧 fingerprint 与当前结果明显不匹配。
- `subname_split_requires_reselect`：旧 face 被切成多个当前 face，默认要求用户重选。
- `subname_deleted`：旧元素对应的几何区域已删除。

旧 face 被新增 cutter 切碎成多个 face 时，默认不选最大重叠面，也不选离中心最近面；这属于产品策略，除非后续明确支持多 face selection。

## CAD Core 落点

### document

扩展 `cad-core/include/cad_core/document/model.h` 的 `Link`：

- 增加 `shadowSubs`，表达 FreeCAD `ShadowSub newName/oldName`。
- 增加 `referenceShadows`，与 `subnames` / `stableSubnames` 对齐。
- `cad-core/src/document/model.cpp` 负责解析 `ReferenceShadow`，并校验长度不一致、字段类型错误和 BREP 格式错误。

建议新增类型：

- `cad-core/include/cad_core/document/reference_shadow.h`
- `cad-core/src/document/reference_shadow.cpp`

### geometry

新增低层能力：

- `cad-core/include/cad_core/geometry/brep_snapshot.h`
- `cad-core/src/geometry/brep_snapshot.cpp`

职责：

- 只处理单个 subshape 的 BREP 序列化 / 反序列化。
- 支持 text BREP 作为调试格式，binary BREP + zstd + base64 作为后续传输格式。
- 不暴露给 feature executor 作为建模输入。

### topo

新增引用恢复匹配器：

- `cad-core/include/cad_core/topo/reference_matcher.h`
- `cad-core/src/topo/reference_matcher.cpp`

职责：

- 从 `NamedShape` / `ElementMap` 解析当前候选。
- 计算当前 subshape fingerprint。
- 实现 FreeCAD-like `findSubShapesWithSharedVertex()` 搜索。
- 返回 `Unique` / `Missing` / `Ambiguous` / `Drift`，不在 matcher 内部改 document graph。

### runtime

`recompute` 输出通过 `elementReferenceUpdates` 给出引用更新建议，不单独新增 `referenceShadowUpdates` 顶层字段。更新项应尽量保持和正式 `PropertyLinkSub` 输入同形，避免出现另一套 patch DSL：

```json
{
  "elementReferenceUpdates": [
    {
      "object": "Pad",
      "property": "Profile",
      "PropertyType": "App::PropertyLinkSub",
      "value": "Sketch",
      "SubList": ["InternalFace3"],
      "StableSubList": ["g305:split3;..."],
      "ShadowSub": [{ "newName": "g305:split3;...", "oldName": "InternalFace2" }],
      "ReferenceShadow": [
        {
          "target": "Sketch",
          "targetId": 30,
          "property": "InternalShape",
          "shapeType": "Face",
          "indexed": "Face3",
          "subname": "InternalFace3",
          "stableSubname": "g305:split3;...",
          "fingerprint": {},
          "brep": {
            "format": "brep-bin-zstd-base64",
            "byteLength": 812,
            "sha256": "base16-or-base64-digest",
            "data": "..."
          }
        }
      ]
    }
  ]
}
```

runtime 只汇总结果；具体解析和匹配逻辑仍归 `topo`。`ReferenceShadow.brep` 在响应里仍然只是写回建议的一部分，只有前端应用到本地 `Objects[]` 后才会成为下一次请求的引用恢复证据。

### features

`features/feature_extrude.cpp` 只消费解析后的 profile face：

- 不在 Pad / Pocket executor 中按旧 subname 猜测。
- 不在 executor 中实现 fingerprint 或旧 BREP 匹配。
- 不因为 `ReferenceShadow` 存在而绕过 `Profile.SubList` / `StableSubList`。

## 分阶段实现

### 阶段一：FreeCAD 基线对齐

- 解析并保存 `ShadowSub newName/oldName`。
- 在 `NamedShape` / `ElementMap` 支持后，用 `StableSubList` 正常恢复 `InternalFaceN`。
- 当引用缺失或 element map 需要反向更新但还没有 BREP matcher 时，先返回明确诊断，不做几何猜测。
- 只在唯一候选时更新引用。

验收：

- `InternalFaceN` 重排但几何区域未变时，Pad 仍恢复到同一区域。
- 目标面删除时返回 missing / deleted。
- 多候选时返回 ambiguous。

### 阶段二：fingerprint 防漂移

- 捕获被引用 subshape 的 fingerprint。
- 稳定名解析成功后做轻量校验。
- 明显不匹配时返回 `subname_semantic_drift`，不静默执行错误 Pad。

验收：

- 旧 stable subname 仍能解析但区域语义变化时，后端返回 drift diagnostic。
- fingerprint 容差按模型尺度处理，大模型和小模型都不过度误报。

### 阶段三：旧子元素 BREP snapshot

- 只对被引用 subshape 保存可选 BREP。
- 实现 FreeCAD-like shared-vertex matcher。
- BREP 读取失败时降级到 fingerprint，不让请求整体崩溃。
- `targetId`、`byteLength` 和 `sha256` 必须先通过校验，再进入 BREP 解码和匹配。

验收：

- 新增 cutter 只改变 split 编号，旧区域几何仍完整存在时，能恢复正确区域。
- 旧 face 被 split 成多个 face 时，返回 split / ambiguous，不静默选一块。
- BREP snapshot 不进入 display、pick、boolean、extrude 的建模输入路径。

## 风险

- BREP snapshot 会增加前端文档体积，必须只保存被引用元素。
- BREP 跨 OCCT 版本读取可能不稳定，需要允许降级到 fingerprint。
- fingerprint 容差过松会误恢复，过严会误报 drift，需要按模型尺度配置。
- 旧 face split / merge 后自动猜用户意图风险高，默认应要求用户重选。
- 如果把旧 BREP 当作建模输入，会破坏 `DocumentObject graph` 唯一真实数据原则。

## 与草图到拉伸方案的关系

本文服务于 `docs/草图实现/5-31-09-28-草图到拉伸补齐方案.md` 的阶段三：

- `InternalShape` 先要有正式 `NamedShape` / `ElementMap`。
- FaceMaker / WireJoiner history 先要进入 `ElementMap`。
- `ReferenceShadow` 是 `StableSubList` 的防漂移和恢复补充，不替代 topo naming 主路径。
- 第一版可以只实现 fingerprint diagnostic；旧 BREP snapshot 已经进入接口，但仍可按实现成本分阶段落地。
