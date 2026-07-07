# RevolvePreviewBody 重复 Vertex stableSubname 修复方案

## 当前结论

这是 `/Users/li/Chili3DProject/FreeCAD/cad-core` 的 Body response stable subname 发布问题。原始建模链本身能被 FreeCAD 生成实体；当前 cad-core 在 response 阶段发现 `RevolvePreviewBody` 同时给 `Vertex1`、`Vertex2` 发布同一个 `RevolvePreview.Vertex1`，于是返回 `duplicate_stable_subname` 并丢弃 `results`。

当前失败不是前端问题，也不应在前端通过猜测 Vertex 名称绕过。修复应对照 FreeCAD 的 `Body::execute()`、`TopoShape::makeShapeWithElementMap()` 和 `MapperMaker` 历史映射，把 cad-core 的 Revolution/Body NamedShape 映射和 response 发布规则修正到同一个语义边界。

## 复现基线

原始输入来自：

```text
/Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json
```

在复制后的 FreeCAD 主线中复现：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
./build/cad-core recompute \
  /Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json \
  --output /tmp/freecad-cad-core-18-53-34-01.json
```

当前输出：

```json
{
  "diagnostics": [
    {
      "code": "duplicate_stable_subname",
      "message": "RevolvePreviewBody publishes duplicate Vertex stableSubname RevolvePreview.Vertex1 for Vertex1, Vertex2",
      "object": "RevolvePreviewBody",
      "severity": "error",
      "stage": "response",
      "subname": "Vertex1, Vertex2",
      "target": "RevolvePreview.Vertex1"
    }
  ],
  "results": []
}
```

这是一条足够紧的红线命令：它不依赖 HTTP、不依赖前端，也能直接命中当前 bug 的 response 发布阶段。

## FreeCADCmd 对照结论

原始 DTO 不能直接作为 FreeCADCmd oracle 输入，因为它包含 cad-core 专用的草图稳定面 token：

```text
g100001;SKT;FAC
```

直接喂给 FreeCADCmd 会失败：

```text
Pad: Sub shape not found: CadCoreExpected#草图_6_24_43_PM.g100001;SKT;FAC
target object RevolvePreviewBody has no shape
```

这只说明 native collector 需要先把 Web/cad-core DTO 归一化为 FreeCAD 原生可理解的 LinkSub 输入，不能说明 FreeCAD 不能生成该形体。

用 native-friendly 探测输入验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
jq '(.Objects[] | select(.Name == "Pad").Properties.Profile.SubSet[0].StableSubList) = []
    | (.Objects[] | select(.Name == "Pad").Properties.Profile.SubSet[0].SubList) = []
    | (.Objects[] | select(.Name == "RevolvePreview").Properties.ReferenceAxis.StableSubList) = []' \
  /Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json \
  > /tmp/18-53-34-01.freecad-native-probe.json

python3 tools/compare_recompute_with_freecadcmd.py \
  /tmp/18-53-34-01.freecad-native-probe.json \
  --output /tmp/freecad-cad-core-18-53-34-01.json \
  --target RevolvePreviewBody \
  --native-out /tmp/18-53-34-01.native-probe.freecad.json \
  --report /tmp/18-53-34-01.native-probe.compare.json \
  --skip-cad-core-shape-summary
```

FreeCADCmd 能输出 `RevolvePreviewBody`：

```json
{
  "object": "RevolvePreviewBody",
  "freecad_version": "1.2.0 revision 20260519",
  "bbox": {
    "min": [-1020.2505384818352, -600.2434472173666, -586.5889817508346],
    "max": [-364.9219352341747, 713.4642716350535, 586.5889817508346]
  },
  "topology_counts": {
    "faces": 15,
    "edges": 29,
    "vertices": 14
  },
  "volume": 476811226.5971333
}
```

比较报告当前仍然红，原因是 cad-core 没有 `results`：

```text
targets.RevolvePreviewBody.status = missing_cad_core
```

修复后的第一目标是让这条比较进入真实几何比较，而不是停在 `missing_cad_core`。

## FreeCAD 语义依据

需要对照的 FreeCAD 源码：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
  - `Body::execute()` 从 `Tip` 读取 `Part::Feature::Shape`，然后 `Shape.setValue(tipShape)`。
  - 语义：Body 的显示 shape 是 Tip shape，不是一个新的、独立的 Vertex/Edge/Face 稳定命名空间。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Body.cpp`
  - `Body::getSubObject()` 最终委托 `Part::BodyBase::getSubObject()`，并保留 Body 内特征 child path 解析能力。
  - 语义：Body result 的 response 可以保持 `id/indexed` 为 Body 当前显示枚举，但 `subname/stableSubname` 必须能回到真实 Tip child path。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRevolution.cpp`
  - `Revolution::makeShape()` 按 `FuseOrder` 决定 `revolve.makeElementFuse(base)` 或 `base.makeElementFuse(revolve)`。
  - 语义：当前 case 的 `RevolvePreview` 是 Tip 形体和 Body replay 的最终 owner。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp`
  - `TopoShape::makeShapeWithElementMap()` 先 `mapSubElement(shapes)`，再消费 maker history。
  - `MapperMaker::modified()` / `generated()` 从 OCCT maker 读取 `Modified(s)` / `Generated(s)`。
  - 语义：稳定名不是简单把当前 `VertexN` 加上 owner 前缀；需要从 maker history 和源 shape ElementMap 中选择可唯一发布的身份。

## cad-core 当前相关落点

优先检查这些实现：

- `cad-core/src/part/topo_shape_expansion.cpp`
  - `makeElementRevolveFromSource()` 使用 `BRepPrimAPI_MakeRevol` 后调用 `namedShapeForMakerHistory()`。
- `cad-core/src/part/topo_shape.cpp`
  - `namedShapeForMakerHistory()` 收集 source element、`Generated`、`Modified`，再 `applyHistoryElementMap()`、`propagateNestedSourceHistory()`、`addMergeHistory()`。
- `cad-core/src/part_design/body.cpp`
  - `directTipSubshapeOwnerForBody()` 判断 Body response 是否可直接用 Tip owner。
  - `addDirectTipSubshapeAliases()` 给 Body NamedShape 增加 `RevolvePreview.VertexN` 这类 Tip child alias。
- `cad-core/src/runtime/recompute.cpp`
  - `stableSubnameFor()` 从 `NamedShape.elementMap` 为当前 `indexed` 选 stable name。
  - `bodyTipQualifiedStableSubname()` 给 Body Tip response 加 owner 前缀。
  - `stableSubnamePublicationConflicts()` 检测同一 kind 下多个 indexed 发布同一个 stableSubname。
  - `requiresStableSubnamePublicationDiagnostics()` 当前只对 Body Tip 为 `PartDesign::Revolution` 的 response 启用重复 stable name 拦截。

当前错误表明：在 `RevolvePreviewBody` 的 response subshapes 中，`Vertex1` 和 `Vertex2` 经过 `stableSubnameFor()` 与 Body Tip owner 规则后都变成了 `RevolvePreview.Vertex1`。这通常意味着 `NamedShape.elementMap` 中有多个 current vertex 共享了同一个 source alias，或者 `addDirectTipSubshapeAliases()` 把不唯一的 current/alias 关系发布成了唯一稳定名。

## 排查假设

按优先级处理：

1. `NamedShape.elementMap` 已经在 Revolution maker history 阶段把两个当前 Vertex 映射到同一个 `Vertex1` 来源。
   - 预测：在 `RevolvePreview` 的 named shape 中，能看到两个不同 current `VertexN` 对应同一个 source stable alias。
   - 修复方向：在 `namedShapeForMakerHistory()` / history 应用阶段，把同一 source vertex 生成多个 target vertex 标为 split 或 merge，不作为单一 stableSubname 发布。
2. `addDirectTipSubshapeAliases()` 对 Body final shape 增加 `RevolvePreview.VertexN` alias 时，没有过滤一对多 current 映射。
   - 预测：去掉或过滤 direct-tip alias 后，response 不再产生重复 `RevolvePreview.Vertex1`，但可能丢失部分 Vertex stableSubname。
   - 修复方向：只为一对一、同 kind、current 唯一的元素增加 direct-tip alias；一对多时保留 `sourceStableSubname` 或 diagnostic evidence，不发布为 `stableSubname`。
3. `stableSubnameFor()` 的优先级规则把 source alias 错选为比当前 Tip-local alias 更稳定。
   - 预测：同一个 current vertex 存在多个候选 stable name，当前优先级选择了可重复的 source alias，而不是唯一的 current Tip-local alias或空 stable。
   - 修复方向：选择 stableSubname 前先做反向唯一性检查；若候选 stable name 会映射到多个 current indexed，则降级，不发布为 stableSubname。
4. `bodyTipQualifiedStableSubname()` 把本应降级为空或 source-only evidence 的 stableSubname 强制加上了 Tip owner。
   - 预测：进入该函数前已有重复候选；函数只是在重复候选外面加了 `RevolvePreview.`。
   - 修复方向：不要在该函数里修历史；在调用前决定候选是否可发布。该函数只负责 owner-qualified 路径拼接。

## 推荐修复步骤

### 1. 固化真实失败 fixture

新增 fixture，建议放入：

```text
cad-core/fixtures/c5m1/revolve-preview-body-duplicate-vertex-stable.json
```

内容直接来自：

```text
/Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json
```

第一条测试先断言当前 bug：

```text
recompute target = RevolvePreviewBody
diagnostics 不应包含 duplicate_stable_subname
results 中必须有 RevolvePreviewBody
RevolvePreviewBody topology_counts 至少应接近 FreeCADCmd: faces=15, edges=29, vertices=14
```

不要只把目标改成 `RevolvePreview`。当前已有 `revolve-preview-body-render-normal` 的测试只覆盖直接输出 feature shape，没有覆盖 Body response 发布。

### 2. 给 native oracle 增加 DTO 归一化说明或 fixture 副本

原始 JSON 中的 `g100001;SKT;FAC` 是 cad-core sketch internal face stable token，不是 FreeCAD 原生 LinkSub subname。收集 FreeCAD oracle 时需要使用 native-friendly 输入：

- `Pad.Profile.SubSet[0].StableSubList = []`
- `Pad.Profile.SubSet[0].SubList = []`
- `RevolvePreview.ReferenceAxis.StableSubList = []`
- `RevolvePreview.ReferenceAxis.SubList = ["Edge1"]`

这不是修业务语义，只是 oracle 采集输入归一化。方案实现时可以选择：

- 在 fixture 旁边记录 native-probe 输入和 expected；
- 或增强 `tools/compare_recompute_with_freecadcmd.py` / `collect_freecad_expected.py` 的归一化逻辑，让 `;SKT;FAC` sketch face stable token 在 native oracle 中退回 whole sketch profile。

本次核心 bug 不应通过修改 expected 或放弃原始 DTO 来掩盖。

### 3. 为 NamedShape 增加 stableSubname 可发布性检查

新增一个内部 helper，位置优先放在 `cad-core/src/runtime/recompute.cpp` 或更底层的 `part/topo_shape.*`：

```text
stableSubnamePublishable(indexed, stableSubname, namedShape)
```

规则：

- 空 stableSubname 不发布。
- kind 必须匹配：Face 只能对应 Face，Edge 只能对应 Edge，Vertex 只能对应 Vertex。
- 同一个 `stableSubname` 在同一个 response object、同一个 kind 下只能对应一个 current `indexed`。
- 如果 `NamedShape.elementMap` 表明同一 stable source 对应多个 current indexed，不能把它作为 `stableSubname` 发布；可以保留在 `sourceStableSubname`、`fragmentStableSubname` 或 mapper history diagnostics 中。

这一步是通用规则，不要写成只识别 `RevolvePreview.Vertex1` 或 fixture 名。

### 4. 修正 direct-tip alias 生成

重点改 `cad-core/src/part_design/body.cpp::addDirectTipSubshapeAliases()`。

当前它会遍历 `namedShape.elementMap`，把 local `Vertex1 -> Vertex1` 变成 `RevolvePreview.Vertex1 -> Vertex1`。需要增加反向唯一性：

```text
只为 stableName/currentName 一对一的 local topological element 添加 Tip alias。
如果同一个 stableName 对应多个 currentName，不添加 Tip alias。
如果同一个 currentName 已有更具体 source alias，也不要让低质量 alias 覆盖。
```

对 Body direct Tip 来说，Face/Edge/Vertex 的 `subname` 仍应是：

```text
RevolvePreview.FaceN
RevolvePreview.EdgeN
RevolvePreview.VertexN
```

但 `stableSubname` 可以为空或降级为唯一可证明的 alias。不要为了通过重复检测而给两个 Vertex 硬改成不同名字；稳定名必须来自可证明的一对一映射。

### 5. 调整 response 发布而不是删除诊断

不要删除 `stableSubnamePublicationConflicts()`，也不要关闭 `requiresStableSubnamePublicationDiagnostics()`。这个诊断现在正好拦住了错误响应。

正确修复后：

- `stableSubnamePublicationConflicts()` 继续存在；
- `RevolvePreviewBody` 不再触发重复 stableSubname；
- 如果未来别的 Revolution Body 又产生重复 stableSubname，仍应诊断失败。

### 6. 回归测试分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
./build/cad-core recompute \
  /Users/li/Chili3DProject/cad-web-background/temp/input/18-53-34-01.json \
  --output /tmp/freecad-cad-core-18-53-34-01.after.json

python3 -m unittest tests.test_p7_features.PartDesignFeatureTest.test_c5m1_revolve_preview_body_duplicate_vertex_stable_subname
```

如果新增测试类或方法名不同，以实际测试名为准，但必须覆盖 `RevolvePreviewBody` target，不是只覆盖 `RevolvePreview`。

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.AdapterTest.test_c_api_body_additive_chain_tip_subshapes_publish_tip_qualified_stable_names
python3 -m unittest tests.test_adapters.AdapterTest.test_c_api_body_replacement_tip_subshapes_publish_tip_qualified_stable_names
python3 -m unittest tests.test_p7_features.PartDesignFeatureTest.test_c5m1_revolve_preview_prefers_stable_axis_over_sketch_internaledge_handle
```

Native oracle 对照：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/compare_recompute_with_freecadcmd.py \
  /tmp/18-53-34-01.freecad-native-probe.json \
  --output /tmp/freecad-cad-core-18-53-34-01.after.json \
  --target RevolvePreviewBody \
  --native-out /tmp/18-53-34-01.native-probe.freecad.after.json \
  --report /tmp/18-53-34-01.native-probe.compare.after.json \
  --skip-cad-core-shape-summary
```

重型收口仅在改动 `namedShapeForMakerHistory()` 或 maker history 传播后执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
python3 -m unittest tests.test_p7_features.PartDesignFeatureTest
```

## 验收标准

修复完成后必须同时满足：

1. 原始输入 `18-53-34-01.json` 重算不再返回 `duplicate_stable_subname`。
2. `results[]` 中存在 `object == "RevolvePreviewBody"`。
3. `RevolvePreviewBody.subshapes[]` 中同一 `kind` 下不存在重复的非空 `stableSubname`。
4. `subname` 对 Body Tip 仍以 `RevolvePreview.` 开头，保持后续 `PropertyLinkSub` 可剥离。
5. 对无法证明唯一稳定身份的 Vertex，宁可不发布 `stableSubname`，也不能发布重复稳定名。
6. FreeCADCmd native-friendly oracle 能生成 `faces=15, edges=29, vertices=14` 的实体摘要；cad-core 至少进入真实几何比较阶段，不再是 `missing_cad_core`。
7. 不改前端，不改原始 DTO 语义，不删除重复 stableSubname 诊断。

## 非目标

- 不在 `/Users/li/Chili3DProject/my-chili3d` 中新增兼容分支。
- 不把 `g100001;SKT;FAC` 伪装成 FreeCAD 原生 subname。
- 不通过 fixture 名称、对象名 `RevolvePreviewBody`、或字符串 `Vertex1` 写特判。
- 不删除 `stableSubnamePublicationConflicts()` 这类 response 合同保护。
- 不修改 FreeCAD expected 来掩盖 cad-core 无结果的问题。

## 建议实现顺序

1. 把原始输入加入 fixture，并补一个当前会红的 Body target 回归。
2. 在测试里先断言 duplicate diagnostic 消失、Body result 存在、stableSubname 无重复。
3. 在 `addDirectTipSubshapeAliases()` 或 stableSubname 选择阶段加入一对一发布检查。
4. 如果发现重复已经来自 `namedShapeForMakerHistory()`，把修复下沉到 `part/topo_shape.cpp` 的 history 传播，不在 response 层猜测。
5. 保留 `sourceStableSubname` / history evidence，让后续拓扑恢复仍能解释该 Vertex 来源，但不要把一对多 source alias 当作 stableSubname 发布。
6. 跑本轮短跑和阶段回归。
7. 如果改了 maker history，追加 native oracle 对照和 expected fixture 阶段回归。
