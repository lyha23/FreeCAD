# RevolvePreviewBody 重复 stableSubname 排查流程

## 修复逻辑

这次 bug 的症状是 `RevolvePreviewBody` 的 response 阶段报：

```text
duplicate_stable_subname
RevolvePreviewBody publishes duplicate Vertex stableSubname RevolvePreview.Vertex1 for Vertex1, Vertex2
```

核心修复不是删除重复项，也不是关闭 diagnostic，而是修正 `Body` direct Tip alias 的来源。

FreeCAD 的语义是：

1. `src/Mod/PartDesign/App/Body.cpp::Body::execute()` 从 `Tip` 取 `Part::Feature::Shape`。
2. 然后 `Shape.setValue(tipShape)`，也就是 Body 显示的是 Tip 的 shape。
3. `Body::getSubObject()` 最终委托 `Part::BodyBase::getSubObject()` 解析 child path。
4. 所以 `RevolvePreviewBody` 对外发布 `RevolvePreview.Vertex1` 时，应指向当前 Tip shape 的 `Vertex1`，不能沿用 maker history 中已经变成别的 current vertex 的旧 local stable name。

修复落点是：

```text
cad-core/src/part_design/body.cpp::addDirectTipSubshapeAliases()
```

原逻辑使用：

```text
tipOwner + "." + stableName
```

这会在本 case 中生成交叉 alias：

```text
RevolvePreview.Vertex2 -> Vertex1
RevolvePreview.Vertex1 -> Vertex2
```

修复后使用：

```text
tipOwner + "." + currentName
```

最终发布为：

```text
RevolvePreview.Vertex1 -> Vertex1
RevolvePreview.Vertex2 -> Vertex2
```

`sourceStableSubname` 和 `stableSubname` 要分清：

- `stableSubname` 是当前 response 可发布、可拾取的稳定子名。
- `sourceStableSubname` 是这个元素能追到的上游历史来源。
- `Vertex1` 能追到 `Fillet.Vertex5`，所以 response 可带 `sourceStableSubname=Fillet.Vertex5`。
- `Vertex2` 没有可证明的上游历史来源，所以不应伪造 `sourceStableSubname`。

## 排查流程

### 1. 先固定红线输入

不要先看代码猜。先找到真实失败请求和失败输出。

本次真实输入：

```text
/Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json
```

红线命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
./build/cad-core recompute \
  /Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json \
  --output /tmp/revolve-preview-body.before.json
```

先确认三件事：

1. `diagnostics[].code` 是否包含 `duplicate_stable_subname`。
2. `results[]` 是否为空。
3. diagnostic 的 `object/target/subname` 是否指向同一个目标问题。

本 case 的目标问题必须是：

```text
object = RevolvePreviewBody
target = RevolvePreview.Vertex1
subname = Vertex1, Vertex2
```

如果这里不是这个症状，不要套用本流程继续修。

### 2. 把真实输入固化成 regression fixture

把原始请求复制到 fixture，保证后续不会只靠临时 `/tmp` 文件复现。

本次 fixture：

```text
cad-core/fixtures/c5m1/revolve-preview-body-duplicate-vertex-stable.json
```

测试必须覆盖 `RevolvePreviewBody` target，不要只测 `RevolvePreview`。因为 bug 发生在 Body response 发布阶段，不是 Revolution feature 自己的基础几何输出阶段。

测试至少断言：

1. 不再有 `duplicate_stable_subname`。
2. `results[]` 中有 `RevolvePreviewBody`。
3. 同一 `kind` 下没有重复的非空 `stableSubname`。
4. Body subshape 的 `subname` 仍以 `RevolvePreview.` 开头。
5. `Vertex1.stableSubname == "RevolvePreview.Vertex1"`。
6. `Vertex2.stableSubname == "RevolvePreview.Vertex2"`。

### 3. 再对照 FreeCAD 源码，不从 cad-core 输出倒推

先看 FreeCAD 的 Body 语义：

```text
src/Mod/PartDesign/App/Body.cpp::Body::execute()
```

关键点：

```text
tipShape = static_cast<Part::Feature*>(tip)->Shape.getShape()
Shape.setValue(tipShape)
```

然后看：

```text
src/Mod/PartDesign/App/Body.cpp::Body::getSubObject()
```

关键点是最终委托 `Part::BodyBase::getSubObject()`。这说明 Body child path 应能按 Tip 子对象路径解析。

再看 Revolution 生成最终 shape 的路径：

```text
src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::setResult()
src/Mod/PartDesign/App/FeatureRevolution.cpp::Revolution::makeShape()
```

要确认 `RevolvePreview` 是 Body Tip 的最终 owner，而不是前端临时命名或 response 层新造 owner。

### 4. 用 FreeCADCmd 做 native 行为验证

如果 DTO 里含有 cad-core 专用 stable token，例如：

```text
g100001;SKT;FAC
```

不要直接把它当 FreeCAD 原生 subname。先做 native-friendly 归一化，只用于 oracle probe，不改变原始 fixture：

```bash
cd /Users/li/Chili3DProject/FreeCAD
jq '(.Objects[] | select(.Name == "Pad").Properties.Profile.SubSet[0].StableSubList) = []
    | (.Objects[] | select(.Name == "Pad").Properties.Profile.SubSet[0].SubList) = []
    | (.Objects[] | select(.Name == "RevolvePreview").Properties.ReferenceAxis.StableSubList) = []
    | (.Objects[] | select(.Name == "RevolvePreview").Properties.ReferenceAxis.SubList) = ["Edge1"]' \
  cad-core/fixtures/c5m1/revolve-preview-body-duplicate-vertex-stable.json \
  > /tmp/revolve-preview-body-native-probe.json
```

然后用 probe 验证 FreeCAD 原生 child path：

```text
RevolvePreviewBody.resolveSubElement("RevolvePreview.Vertex1") -> Vertex1
RevolvePreviewBody.resolveSubElement("RevolvePreview.Vertex2") -> Vertex2
```

如果原生 FreeCAD 也是交叉的，cad-core 不应改。如果原生 FreeCAD 是 `Vertex1 -> Vertex1`、`Vertex2 -> Vertex2`，cad-core 的交叉 alias 就是 bug。

### 5. 再看 cad-core 的发布链

按这个顺序看代码：

1. `cad-core/src/part_design/body.cpp::getBodyTopoShapeAtFeature()`
   - 看 Body replay 到哪个 Tip。
   - 看 `directTipSubshapeOwner` 是否是 `RevolvePreview`。
2. `cad-core/src/part_design/body.cpp::addDirectTipSubshapeAliases()`
   - 看 `NamedShape.elementMap` 里 local element 如何变成 `RevolvePreview.VertexN`。
   - 本 bug 的根因就在这里把 `stableName` 当成 direct Tip child path。
3. `cad-core/src/runtime/recompute.cpp::stableSubnameFor()`
   - 看 response 如何从 `NamedShape.elementMap` 选择 stable name。
4. `cad-core/src/runtime/recompute.cpp::bodyTipQualifiedStableSubname()`
   - 看 Body response 如何加 Tip owner 前缀。
5. `cad-core/src/runtime/recompute.cpp::stableSubnamePublicationConflicts()`
   - 看重复 stableSubname diagnostic 是否正常拦截坏输出。

不要先改 `stableSubnamePublicationConflicts()`。它是保护合同的诊断，不是 bug 根因。

### 6. 打印或检查 NamedShape elementMap

用 legacy 输出看真实 alias：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
CAD_CORE_TEST_LEGACY_OUTPUT=1 ./build/cad-core recompute \
  fixtures/c5m1/revolve-preview-body-duplicate-vertex-stable.json \
  --output /tmp/revolve-preview-body.legacy.json

jq '(.named_shapes.RevolvePreviewBody.element_map
     // .named_shapes.RevolvePreviewBody.elementMap
     // {})
    | to_entries
    | map(select(.value=="Vertex1" or .value=="Vertex2"))
    | sort_by(.value,.key)' \
  /tmp/revolve-preview-body.legacy.json
```

坏输出会看到：

```text
RevolvePreview.Vertex2 -> Vertex1
RevolvePreview.Vertex1 -> Vertex2
```

修复后应看到：

```text
RevolvePreview.Vertex1 -> Vertex1
RevolvePreview.Vertex2 -> Vertex2
```

这一步能判断问题是在 `elementMap` 构造阶段，还是只在 response 选择阶段。

### 7. 落修复时保持边界

正确边界：

- `Body` direct Tip alias 负责发布当前 Tip child path。
- maker history 继续负责 source provenance。
- response diagnostic 继续负责拦截重复 stableSubname。

不要做这些事：

- 不删除重复 diagnostic。
- 不在 response 层把重复项清空来绕过错误。
- 不按 fixture 名、对象名或 `Vertex1/Vertex2` 写特判。
- 不伪造 `sourceStableSubname`。
- 不把 `sourceStableSubname` 当成当前可发布的 `stableSubname`。

### 8. 跑最小验证

本轮最小验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c5m1_revolve_preview_body_duplicate_vertex_stable_subname
```

同时用原始输入复跑：

```bash
./build/cad-core recompute \
  /Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json \
  --output /tmp/revolve-preview-body.after.json

jq '{diagnostics:.diagnostics, resultObjects:[.results[].object]}' \
  /tmp/revolve-preview-body.after.json
```

期望：

```json
{
  "diagnostics": [],
  "resultObjects": ["RevolvePreviewBody"]
}
```

### 9. 跑邻近回归

这次改的是 Body Tip alias，容易影响已有 Body direct Tip contract，所以至少跑：

```bash
python3 -m unittest \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names

python3 -m unittest \
  tests.test_adapters.CadCoreAdapterTest.test_c_api_body_additive_chain_tip_subshapes_publish_tip_qualified_stable_names

python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c5m1_revolve_preview_prefers_stable_axis_over_sketch_internaledge_handle

git diff --check
```

直接 Tip 旧 contract 仍应保持：

```text
Face5.subname = Pad.Face5
Face5.stableSubname = Sketch.Face1
```

这说明当前显示路径和更早的 source stable identity 没被混掉。

## 最终验收口径

修复完成才算过关：

1. 原始输入不再触发 `duplicate_stable_subname`。
2. `results[]` 返回 `RevolvePreviewBody`。
3. `RevolvePreviewBody` 同一 kind 下没有重复非空 `stableSubname`。
4. `Vertex1` 发布 `RevolvePreview.Vertex1`，`Vertex2` 发布 `RevolvePreview.Vertex2`。
5. `sourceStableSubname` 只在有上游来源证据时出现。
6. `stableSubnamePublicationConflicts()` 保留，未来仍能拦截错误发布。
7. 不改前端，不改原始 DTO，不用对象名或 fixture 名特判。
