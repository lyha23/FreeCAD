# P1：Sketch + Body + Pad 建模闭环

P1 的目标是生成第一份真实几何结果。范围只覆盖一个闭合草图通过 Body 里的 Pad 拉伸成实体，然后导出 mesh 和 subshape map。

## 当前状态

当前 `/cad-core` 已完成 P1 基线：`Sketcher::SketchObject -> PartDesign::Pad -> PartDesign::Body` 可以生成真实 OCCT 结果，并导出 mesh、bbox、volume、subshape map。

这个 P1 只能证明最小闭环，不能等同于完整 PartDesign Body 迁移：

- Body 只覆盖 `Sketch -> Pad` 最小顺序和 Tip。
- Pad 只覆盖 `Length` / `One side` 的最小拉伸。
- 没有 `FeatureBase` 前序 solid，也没有 `FeatureAddSub` 的 add/sub 通道。
- 没有 Pocket、Hole、Pattern、Mirror、Fillet、Chamfer。

## FreeCAD 语义参考

实现时优先看这些 App / Mod 层文件，不从 Gui 层迁移依赖：

| 能力 | FreeCAD 参考位置 |
| --- | --- |
| 文档对象 recompute 外壳 | `src/App/Document.cpp`、`src/App/DocumentObject.cpp` |
| 链接属性 | `src/App/PropertyLinks.cpp`、`src/App/PropertyLinks.h` |
| Placement | `src/App/GeoFeature.cpp`、`src/App/GeoFeatureGroupExtension.cpp` |
| Sketch 执行 | `src/Mod/Sketcher/App/SketchObject.cpp` |
| Body 特征链 | `src/Mod/PartDesign/App/Body.cpp`、`src/Mod/PartDesign/App/Body.h` |
| PartDesign 通用特征 | `src/Mod/PartDesign/App/Feature.cpp`、`src/Mod/PartDesign/App/Feature.h` |
| Pad 拉伸 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`、`src/Mod/PartDesign/App/FeaturePad.cpp` |

## Step 7：实现 Sketch 最小 profile

做什么：

- 支持 `Sketcher::SketchObject` executor。
- MVP 只接受已经能成形的二维几何，不先做完整约束求解器。
- 先支持直线闭合轮廓；圆弧、圆、约束求解可以后置。
- 输出 Sketch 的 profile shape，供 Pad 的 `Profile` 使用。

建议 MVP 输入：

```json
{
  "Geometry": [
    { "kind": "LineSegment", "start": [0, 0], "end": [10, 0] },
    { "kind": "LineSegment", "start": [10, 0], "end": [10, 5] },
    { "kind": "LineSegment", "start": [10, 5], "end": [0, 5] },
    { "kind": "LineSegment", "start": [0, 5], "end": [0, 0] }
  ]
}
```

产出：

- Sketch executor。
- 闭合 profile 检查。
- `open-sketch.json` fixture。

验收：

- 闭合矩形能生成 profile。
- 开口轮廓返回 diagnostics，不让 Pad 继续假成功。
- 不支持的几何类型返回 diagnostics。

## Step 8：实现 Body 最小特征链

做什么：

- 支持 `PartDesign::Body` executor。
- 读取 Body 的特征列表和 Tip。
- 按 Body 内顺序执行特征。
- MVP 只要求 `Sketch -> Pad`，BaseFeature 和多特征链先返回明确 diagnostics。

建议 MVP 属性：

```json
{
  "Group": [
    {
      "PropertyType": "App::PropertyLinkSub",
      "value": "Sketch",
      "SubList": []
    },
    {
      "PropertyType": "App::PropertyLinkSub",
      "value": "Pad",
      "SubList": []
    }
  ],
  "Tip": {
    "PropertyType": "App::PropertyLinkSub",
    "value": "Pad",
    "SubList": []
  }
}
```

产出：

- Body 内对象顺序解析。
- Tip 校验。
- Body 坐标和子对象 Placement 的最小合成。

验收：

- Tip 指向不存在对象会报 diagnostics。
- Body 中 Pad 能读取 Sketch profile。
- Body 结果指向 Tip 的最终 shape。

## Step 9：实现 Pad 长度拉伸

做什么：

- 支持 `PartDesign::Pad` executor。
- 读取 `Profile`、`Length`、`Reversed`、`SideType`。
- MVP 只支持 `SideType = "One side"` 的长度拉伸。
- 其他 UpTo、TwoLengths、taper angle 先报 unsupported diagnostics。

建议 MVP 属性：

```json
{
  "Profile": {
    "PropertyType": "App::PropertyLinkSub",
    "value": "Sketch",
    "SubList": []
  },
  "Type": "Length",
  "Length": 10.0,
  "Reversed": false,
  "SideType": "One side"
}
```

产出：

- Pad executor。
- profile face 到 prism solid 的几何调用。
- Pad 结果写入 `ComputeContext`。

验收：

- 矩形 Sketch + Pad 生成实体。
- `Length <= 0` 返回 diagnostics。
- 缺失 Profile 返回 diagnostics。
- 不支持的 SideType 返回 diagnostics。

## Step 10：导出 mesh

做什么：

- 为 recompute 结果添加 mesh 输出。
- 支持按对象名导出 Pad mesh。
- mesh 输出可以先是 JSON 三角网格或单独文件路径。

建议结果字段：

```json
{
  "mesh": {
    "Pad": {
      "summary": {
        "vertex_count": 8,
        "triangle_count": 12
      }
    }
  }
}
```

产出：

- `MeshExporter`。
- `rect-pad.result.json` 中的 `mesh.Pad`。
- mesh 顶点、三角面、包围盒摘要。

验收：

- `mesh.Pad.summary` 存在，顶点和三角面非空。
- 包围盒和 Pad 长度一致。
- 不存在对象不能导出 mesh。

## Step 11：导出 subshape map

做什么：

- 给 Pad 输出最小 subshape map。
- MVP 可以先使用稳定排序的 `Face1`、`Edge1`、`Vertex1`。
- 后续 topo naming 再升级成可跨编辑恢复的稳定引用。

建议结果字段：

```json
{
  "subshapes": {
    "Pad": {
      "Face1": { "kind": "face" },
      "Edge1": { "kind": "edge" },
      "Vertex1": { "kind": "vertex" }
    }
  }
}
```

产出：

- `SubshapeMapExporter`。
- Pad face / edge / vertex 计数。
- fixture 对照文件。

验收：

- subshape map 不为空。
- 同一输入重复运行 key 顺序稳定。
- 无法命名的子形状要进入 diagnostics。

## Step 12：固化 P1 fixtures

做什么：

- 为每个关键路径补最小输入和期望输出。
- 用 FreeCAD 生成一份对照结果，至少记录包围盒、体积或 mesh 摘要。
- fixtures 只覆盖 MVP，不提前铺开 P2/P3。

建议 fixtures：

```text
fixtures/mvp/
  empty.json
  unknown-type.json
  missing-profile.json
  open-sketch.json
  rect-pad.json
  expected/
    rect-pad.freecad.json
    rect-pad.result.json
```

验收：

- 正常 Pad case diagnostics 为空。
- 错误 case diagnostics code 稳定。
- `rect-pad` 的包围盒、体积或三角面摘要能和 FreeCAD 对照。

## P1 冻结点

P1 之后不要继续把新语义塞进 `Pad` executor：

- `Pad` 只保留 additive feature 的参数读取和入口。
- 拉伸方向、长度、SideType、UpTo* 进入共享 `FeatureExtrude`。
- Body 的 Fuse / Cut 链进入 `FeatureAddSub` 和 Body 执行链。
- Pocket 必须通过 subtractive 通道接入，不要复制一套 Pad 拉伸。
