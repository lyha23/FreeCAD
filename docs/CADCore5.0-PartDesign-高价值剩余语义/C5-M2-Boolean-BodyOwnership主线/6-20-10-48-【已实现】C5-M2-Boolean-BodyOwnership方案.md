# 【已实现】C5-M2 Boolean / Body ownership 方案

## 目标

把 C4 中已支持的 `PartDesign::Boolean` Fuse / Cut / Common Body-tool first slice，扩展到前端 CAD runtime 常见的 Body ownership 压力场景：AllowCompound、multi-solid policy、multi-tool Group、BaseFeature reroute、missing/null tool diagnostics 和 element ownership。

## 范围

- FreeCAD 源码依据：`src/Mod/PartDesign/App/FeatureBoolean.cpp`、`src/Mod/PartDesign/App/Body.cpp`。
- topo 依据：`src/Mod/Part/App/TopoShapeExpansion.cpp`、`src/Mod/Part/App/TopoShape.cpp`。
- cad-core 落点：`cad-core/src/part_design/feature_boolean.*`、`cad-core/src/part_design/body.*`、`cad-core/src/part/topo_shape*`、`cad-core/tools/collect_freecad_expected.py`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_expected_fixtures`、`tests.test_adapters`。

## 最小完整语义批次

本包不只做一个 AllowCompound fixture。合理批次必须同时覆盖：

- AllowCompound false：多 solid result 保持结构化失败，诊断带 object / property / stage。
- AllowCompound true：多 solid result 的 shape、Body Tip replacement、subshape map 和 capability metadata。
- multi-tool Group：多个 Body tool 的顺序、缺失 tool、null shape 和 source ownership。
- BaseFeature / Group / Tip 更新：不破坏 C4 的 Fuse / Cut / Common first slice。
- LinkStage3-only Compound / Section：保持 non-goal 或 unsupported Type diagnostic，不混入本包 supported。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：Boolean Type、Group、BaseFeature、AllowCompound、Body reroute |
| S1 | native oracle：AllowCompound true/false、multi-tool ownership、failure diagnostics |
| S2 | cad-core Body ownership / topo history / diagnostics / capability metadata |
| S3 | focused tests 与 remaining boundary 收口 |

## FreeCAD 调用链记录

- `src/Mod/PartDesign/App/FeatureBoolean.cpp::Boolean::execute()`：读取 `Type`，只接受 `Fuse` / `Cut` / `Common`；`Cut` 没有 `BaseFeature` 时返回 `Cannot do boolean cut without BaseFeature`；读取 `Group` 工具列表，有 `BaseFeature` 时以其 shape 为 base，否则把 `Group` 最后一项作为 base 并从 tools 中移除。
- `Boolean::execute()` 对每个 tool 调 `getTopoShape(... ResolveLink | Transform)`；tool shape 为空时返回 `Tool shape is null`；按 `Type` 映射到 `Part::OpCodes::Fuse/Cut/Common`，调用 `result.makeElementBoolean(op, shapes, nullptr, FuzzyTolerance.getValue())`；`Compound` / `Section` 代码在 LinkStage3 注释中明确 pending decision。
- `src/Mod/PartDesign/App/Feature.cpp::singleSolidRuleMode()`：没有所属 Body 时强制 single-solid；有 Body 时读取 `body->AllowCompound`，`AllowCompound=true` 则关闭 single-solid rule。`Feature::getSolid()` 在 single-solid rule enforced 且只有一个 solid 时返回 `shape.getSubTopoShape(TopAbs_SOLID, 1)`，否则保留原 shape。
- `src/Mod/PartDesign/App/Body.cpp::Body()` 默认 `AllowCompound=true`；`Body::insertObject()` / `setBaseProperty()` 维护 `Group` 顺序、当前 solid 的 `BaseFeature` 和后续 solid 的 reroute；`Body::execute()` 读取 `Tip` 的 `Shape`，并把 Tip shape 作为 Body shape。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementBoolean()` 对 `Fuse` / `Cut` 展开 compound inputs，使用 `BRepAlgoAPI_Fuse/Cut/Common`，再走 `makeElementShape(*mk, inputs, op, elementMapPolicy)`，让 `MapperMaker` 消费 maker history。

## 本轮结果

- supported：`Fuse/Cut/Common` C4 first slice 保持；`AllowCompound=true` 多 solid result 保留 `occt_compound`，Body Tip replacement 和 subshape map 对齐 native expected；`AllowCompound=false` 多 solid result 返回 `multiple_solids_disallowed`，带 object / property / stage / target。
- supported：multi-tool `Group` 顺序 fixture 采 native expected，`tools=["ToolBodyB","ToolBodyA"]` 稳定，single-solid rule enforced 时按 FreeCAD `getSolid()` 提取唯一 solid，不破坏 C4 默认 `AllowCompound=true` compound 输出。
- diagnostic-backed：缺失 tool 在 graph 阶段定位到 `Group` / target；非 solid tool 在 runtime 阶段定位到 `Group` / target；`Section` / `Compound` 仍按 `unsupported_property` 落到 `Type`，保持 `C5-NG-005`。
- cad-core 落点：`cad-core/src/part_design/feature_boolean.cpp` 读取所属 Body 的 `AllowCompound` 并执行 FreeCAD single-solid rule；`cad-core/src/part_design/body.cpp` 接受 `AllowCompound` 并按实际 OCCT shape kind 输出 Body 结果；adapter 只同步 capability metadata。

## 非目标

- 不支持 FreeCAD 源码中已注释掉的 Compound / Section Type，除非产品 scope 单独重开。
- 不把 multi-solid result 修剪成单 solid 来通过 fixture。
- 不在 adapter 中判断 Body ownership。

## 验收

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义 cad-core
```

阶段回归：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```
