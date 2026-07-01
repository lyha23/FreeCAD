# 已有 surface / solid 边继续开放拉伸方案

## 背景

当前 open wire 拉伸扩展已经支持从 raw Sketch edge 继续生成 display-only surface / shell：

- `OpenProfileMode=Auto` 下，closed face 走实体 Pad / Pocket，open wire 走 `SurfaceExtrusion`。
- raw Sketch edge 使用 `StableSubList: ["g<ID>"]` 作为稳定身份。
- display-only open surface 的结果可以作为 feature 自身的 `Shape` 输出，并能通过 Body Tip 显示。

但“第二次从已有 surface / solid 的边继续开放拉伸”还缺 profile acquisition 能力。典型失败形态是：

```json
{
  "Profile": {
    "PropertyType": "App::PropertyLinkSubList",
    "SubSet": [
      {
        "value": "Pad",
        "SubList": ["Edge4"],
        "StableSubList": ["Edge4"]
      }
    ]
  },
  "OpenProfileMode": {
    "PropertyType": "App::PropertyEnumeration",
    "value": "SurfaceExtrusion"
  }
}
```

这里的 `Edge4` 不是 raw Sketch 的 `g<ID>` 边，而是上一个 Pad / surface / solid 结果上的拓扑边。后端如果只把 `EdgeN` 当 raw Sketch open profile 处理，就会误入 Sketch-only 分支，然后报类似：

```text
unsupported_open_profile_multi_target:
open wire profiles currently require a Sketcher::SketchObject target
```

这个问题不是前端单纯改字段能解决的。要支持它，后端必须把“从已有 shape 解析选中边”做成 profile resolver 的正式能力。

## FreeCAD 依据

FreeCAD 的关键不是“Pad native 支持开放边直接并入实体”，而是 profile / subshape 获取层本身是通用的。

相关源码依据：

- `src/Mod/PartDesign/App/FeatureSketchBased.h::ProfileBased::Profile` 是 `App::PropertyLinkSub`。
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getVerifiedObject()` 接受 `Part::Feature`，不是只接受 Sketch。
- `src/Mod/PartDesign/App/FeatureSketchBased.cpp::ProfileBased::getProfileShape()` 对每个 subname 调用 `Part::Feature::getTopoShape(profile, subShapeOptions, sub.c_str())`。
- `src/Mod/Part/App/PartFeature.cpp::Feature::getTopoShape()` / `_getTopoShape()` 负责 `getSubObject()`、Link、Body 子路径和 transform。
- `src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion()` 在 `makeface == false` 时读取 `Profile.getSubValues(false)`，对每个 sub 调 `Part::Feature::getTopoShape(... NeedSubElement | ResolveLink | Transform, sub.c_str())`，如果包含 edge 则 `makeElementWires(shapes)`。
- `src/Mod/Part/App/FeatureExtrusion.cpp::Extrusion::extrudeShape()` 在 `Solid=false` 时不会强制从 wire 造 face，而是直接 `makeElementPrism(myShape, vec)`，因此 edge / wire 可以生成 surface / shell。

因此 cad-core 的实现方向应是：

1. profile acquisition 层支持从任意可选取 shape source 解析 EdgeN / stable edge。
2. open profile execution 层把解析到的 edge / wire 当 `OpenWire` / `EdgeCompound` 拉伸。
3. Pad / Pocket 的实体 Body 参与语义仍保持显式：开放边 surface 默认 `display_only`，不悄悄 fuse / cut。

## 目标

1. 支持 `Profile.SubSet[].value` 指向已有 `surface / solid / display-only Pad / Body Tip` 时，`SubList: ["EdgeN"]` 可以解析为 open profile。
2. 修复 `EdgeN` 被无条件识别成 raw Sketch open edge 的误分类。
3. 复用现有 `ProfileBasedProfileSelection`，不要在 Pad / Pocket executor 里重新解析 `PropertyLinkSubList`。
4. 支持 display-only open surface feature 作为后续 profile source；也就是 `runtime::ShapeValue::Kind::PartPrimitive` 必须可作为 linked-edge source。
5. 复用现有 Body cumulative replay：同一 Body 内引用早期 feature 的边时，应按该 feature 当时的累计 Body shape 解析。
6. 输出继续标明 `profileKind=open_wire` 或 `edge_compound`，并保留 source profile subname / stableSubname 证据。

## 非目标

- 不改变 FreeCAD native Pad / Pocket 的 face-first 实体语义。
- 不把 open surface 默认并入 Body solid。
- 不让前端伪造 `StableSubList: ["EdgeN"]` 当成跨编辑稳定名。
- 不在 executor、runtime response 或 frontend 里猜 `Pad.EdgeN` 前缀。
- 不在本轮支持跨多个 source object 混合边拉伸；多个边必须先限制在同一个 source owner 内。

## 输入合同

### 1. raw Sketch open edge

raw Sketch open edge 继续使用稳定 geometry id：

```json
{
  "value": "Sketch",
  "StableSubList": ["g101", "g102"]
}
```

这条路径仍由 raw sketch resolver 处理。

### 2. 已有 feature / surface / solid 边

已有 shape 的边使用 owner-local subname：

```json
{
  "value": "Pad",
  "SubList": ["Edge4"]
}
```

如果后端 response 已经提供稳定拓扑名，则可以传：

```json
{
  "value": "Pad",
  "SubList": ["Edge4"],
  "StableSubList": ["Generated:Sketch.g102:Side1"]
}
```

如果没有真实稳定名，前端应省略 `StableSubList`，或只把它作为 current-name fallback 兼容处理。`StableSubList: ["Edge4"]` 不能被标记为稳定，只能触发 warning。

### 3. 从 Body Tip 拾取

Body response 中的 `subname/stableSubname` 可能是 Tip-qualified 路径，例如：

```json
{
  "id": "Body:Edge4",
  "subname": "Pad.Edge4",
  "stableSubname": "Pad.Generated:Sketch.g102:Side1"
}
```

推荐前端写回时把 owner 剥离为真实 feature：

```json
{
  "value": "Pad",
  "SubList": ["Edge4"],
  "StableSubList": ["Generated:Sketch.g102:Side1"],
  "FullSubList": ["Pad.Edge4"]
}
```

兼容期后端也可以接受：

```json
{
  "value": "Body",
  "SubList": ["Pad.Edge4"]
}
```

但 resolver 内部必须通过 Body replay 找到 `Pad` 对应的累计 shape，不应把 `Body.Edge4` 当作全局稳定身份。

## 模块设计

### 外部 seam

保持现有接口：

```cpp
std::vector<ProfileBasedProfileSelection> resolveProfileBasedProfilesForExtrusion(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const std::string& featureName,
    OpenProfileMode openProfileMode,
    std::string profileRequirementMessage = {});
```

Pad / Pocket / shared FeatureExtrude 只消费 `ProfileBasedProfileSelection`，不关心 profile 来自 raw Sketch、已有 surface、solid 还是 Body cumulative replay。

### 内部新增分支

在 `cad-core/src/part_design/profile_resolver.cpp` 内新增 linked-edge resolver：

```cpp
std::optional<ProfileBasedProfileSelection> resolveLinkedOpenProfileSelection(
    const app::DocumentObject& object,
    runtime::ComputeContext& context,
    const app::Link& profileLink,
    const runtime::ShapeValue& shapeValue,
    OpenProfileMode openProfileMode,
    const std::string& featureName);
```

该分支内部复用 `resolveFaceOnSource()` 的结构，但目标 shape kind 改为 `TopAbs_EDGE`：

```cpp
ResolveAttempt resolveEdgesOnSource(
    const app::DocumentObject& object,
    const app::Link& profileLink,
    const TopoDS_Shape& sourceShape,
    const part::NamedShape* namedShape,
    const std::string& featureName);
```

解析成功后：

- 多条 edge 能 `BRepBuilderAPI_MakeWire` 成功时，返回 `ProfileKind::OpenWire`。
- 无法组成单 wire 但每条边都合法时，返回 compound，`ProfileKind::EdgeCompound`。
- 保留 `selectedSubnames` / `selectedStableSubnames`。
- 如果只用了 `EdgeN` current name，设置 `unstableOpenProfileReference=true` 并发 warning。

## 关键修复点

### 1. raw Sketch 识别必须先看 source kind

当前风险是：

```cpp
linkRequestsRawOpenSketchProfile(profileLink)
```

只要看到 `EdgeN` 就返回 true。应改成：

- `StableSubList` 含 `g<ID>`：这是 raw Sketch 候选，但仍要验证 target 是 `Sketcher::SketchObject` / `ShapeValue::Kind::Sketch`。
- `SubList` 含 `EdgeN`：不能单独证明是 raw Sketch；必须结合 target kind。
- 如果 target 不是 Sketch，直接跳过 raw Sketch 分支，进入 linked-edge 分支。

建议把判断拆成两个函数：

```cpp
bool linkRequestsRawSketchStableEdge(const app::Link& profileLink);
bool linkRequestsCurrentEdgeSubshape(const app::Link& profileLink);
```

在 `resolveProfileBasedProfilesForExtrusion()` 里根据 `shapeIt->second.kind` 决定分派。

### 2. profile source kind 扩展

`resolveProfileBasedProfileLink()` 当前应接受这些 source：

- `Sketch`：closed face / raw open sketch edge。
- `Profile`：已有 profile shape。
- `Solid`：实体上的 face / edge。
- `PartPrimitive`：surface / shell / display-only Pad / Part workbench 结果。

新增 linked-edge 能力后，`PartPrimitive` 不能再被当成 “did not produce a profile”。

### 3. linked face 和 linked edge 共享 Body replay

已有 `resolveLinkedFaceProfileSelection()` 会：

1. 尝试在直接 feature shape 上解析 FaceN。
2. 如果同 Body 早期 feature 需要 cumulative state，则调用 `getBodyTopoShapeAtFeature()`。
3. 再在 Body-at-feature shape 上解析。

linked-edge 应完全复用这个 replay 策略，只是解析目标从 FaceN 换成 EdgeN。建议抽一个小的内部 helper：

```cpp
template <typename ResolveOnSource>
std::optional<ProfileBasedProfileSelection> resolveLinkedSubshapeProfileSelection(...);
```

如果不想引入 template，也可以先复制结构，但命名必须清楚：`resolveLinkedFaceProfileSelection()` 和 `resolveLinkedOpenProfileSelection()` 是两个同级分支。

### 4. stableSubname 解析规则

优先级：

1. `NamedShape.element_map` 命中 `StableSubList`，解析到当前 `EdgeN`。
2. `StableSubList` 是 target-local current name，例如 `Edge4`，仅当它等于 `SubList` 时作为 fallback，标记 unstable。
3. `StableSubList` 是 Body-qualified 路径，例如 `Pad.Edge4`，先剥离 owner，再交给 Body replay source。
4. `ReferenceShadow` 存在时，按现有 stable recovery 机制恢复。
5. 找不到时返回 `unsupported_stable_subname` / `deleted_stable_subname` / `split_stable_subname`，不得静默落到其它 edge。

## 执行层保持不变

`cad-core/src/part_design/feature_extrude.cpp` 已经有 open profile 分支：

- `ProfileKind::OpenWire`
- `ProfileKind::EdgeCompound`
- `OpenProfileMode::SurfaceExtrusion`
- `bodyParticipation=display_only`
- ThinSolid / ThinCut 的厚度路径

因此本方案不要求 executor 新增一套 “从 Pad.EdgeN 拉伸” 逻辑。只要 resolver 返回正确的 `ProfileBasedProfileSelection`，现有 `buildFeatureExtrusion()` 应继续工作。

## 实施批次

### S1：锁定回归 fixture

新增 fixture，模拟用户场景：

- 第一个 Pad 从 raw Sketch open wire 拉出 display-only surface。
- 第二个 PadPreview / Pad2 的 `Profile.value` 指向第一个 Pad。
- `SubList: ["EdgeN"]` 指向第一个 Pad 输出边。
- `OpenProfileMode=SurfaceExtrusion`。

期望：

- 第二个 feature `status=ok`。
- `profileKind=open_wire` 或 `edge_compound`。
- `sourceProfile.object == "Pad"`。
- diagnostics 不再出现 `unsupported_open_profile_multi_target`。
- 若 `StableSubList=["EdgeN"]`，允许 warning `ambiguous_open_profile_reference`，但不能 error。

建议文件：

- `cad-core/fixtures/p7/partdesign-pad-open-wire-from-pad-edge.json`
- `cad-core/tests/test_p7_features.py::test_p7_pad_open_wire_profile_from_existing_pad_edge`

### S2：修复 raw Sketch 分派

修改 `resolveProfileBasedProfilesForExtrusion()`：

1. 先读取 `profileLink.object` 对应的 `shapeIt`。
2. 只有 target kind 是 `Sketch` 时，才尝试 `resolveRawSketchOpenProfile()`。
3. 非 Sketch 的 `EdgeN` 进入 linked-edge 分支。

验收重点：

- 原有 raw Sketch `g<ID>` 测试全部通过。
- `value=Pad, SubList=Edge4` 不再触发 raw Sketch-only 错误。

### S3：实现 linked-edge profile selection

新增 `resolveLinkedOpenProfileSelection()`：

- 支持 `Solid` / `Profile` / `PartPrimitive`。
- 支持单 edge 和多 edge。
- 使用 `NamedShape` / `ElementMap` 解析 stable edge。
- 没有 stable evidence 时允许 current `EdgeN`，但标记 unstable warning。
- `OpenProfileMode=Reject` 时返回 `open_profile`，保持 strict mode。

Body 场景：

- 如果 target 是同 Body 早期 feature，先尝试 direct feature shape。
- direct feature shape 没有对应 edge 时，按 `getBodyTopoShapeAtFeature()` 重放到该 feature 的累计 Body shape。
- Body-qualified stable/subname 先转换成 owner-local 再解析。

### S4：PartPrimitive source 接入

在 `resolveProfileBasedProfileLink()` 的合法 source kind 中加入 `PartPrimitive`。

这一步是第二次开放拉伸的必要条件，因为 display-only open surface feature 会以 `ShapeValue::Kind::PartPrimitive` 发布。

### S5：响应和诊断收口

新增或复用诊断：

| code                                      | 场景                                                              |
| ----------------------------------------- | ----------------------------------------------------------------- |
| `open_profile`                          | `OpenProfileMode=Reject` 遇到 open edge / wire。                |
| `ambiguous_open_profile_reference`      | 只靠 current`EdgeN` 或 stable edge 无法唯一解析。               |
| `unsupported_subshape_kind`             | profile 指向 Face/Vertex 等不符合当前 open edge 分支的 subshape。 |
| `unsupported_open_profile_multi_target` | 多 source owner 混合 open edge。                                  |
| `unsupported_stable_subname`            | stable edge 不在当前 ElementMap 且无法恢复。                      |

成功结果继续使用已有字段：

```json
{
  "profileKind": "open_wire",
  "openProfileMode": "SurfaceExtrusion",
  "resolvedOpenProfileMode": "SurfaceExtrusion",
  "bodyParticipation": "display_only",
  "sourceProfile": {
    "object": "Pad",
    "stableSubnames": ["Edge4"]
  }
}
```

如果 `stableSubnames` 只是 current-name fallback，结果中应能看出它不是稳定身份，或者至少伴随 warning。

## 验收命令

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_pad_open_wire_profile_from_existing_pad_edge
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_pad_open_wire_profile_auto_surface_extrusion
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_body_tip_open_wire_pad_returns_display_surface
```

如果实现落在 `cad-web-background/cad-core` 当前后端仓库，同样要用该仓库的 CLI 对原始失败 payload 回归：

```bash
cd /Users/li/Chili3DProject/cad-web-background
cad-core/build/cad-core recompute temp/input/22-28-28-01.json --output temp/debug/22-28-28-01.linked-edge.json
```

期望输出中第二次拉伸不再是 `unsupported_open_profile_multi_target`，而是成功生成 display-only surface / shell，或在稳定名不足时只给 warning。
