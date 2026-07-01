# Body 直连 Tip 稳定子名传播修复方案

## 实现状态

已实现。

- `rect-pad` Body response 现在保持 `Face5.subname == "Pad.Face5"`，同时发布 `Face5.stableSubname == "Sketch.Face1"`、`Edge3.stableSubname == "Sketch.Edge1"`。
- Pad 自身 `NamedShape.elementMap` 在 Refine=false 与 Refine=true fixture 中均保留 `Sketch.Face1 -> Face5`、`Sketch.Edge1 -> Edge3`。
- 修复没有在 Body、adapter 或 recompute 层新增 `FaceN -> Sketch.*` 特判；response 只在已有 `NamedShape.elementMap` 候选中区分 source stable alias 与 display alias。
- 同步补齐 Sketch 有 InternalShape 时 raw EdgeN identity publication，避免关闭纯 indexed fallback 后丢失 `identityStatus == "index_fallback"` 合同。

最终验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_refine_false_is_feature_refine_noop tests.test_p7_features.CadCoreP7FeatureTest.test_p7_refine_true_uses_refinemodel_path
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests.test_mvp
git diff --check
```

## 问题定义

当前阶段回归失败项：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names
```

失败合同不是 Body 少返回了子形，而是 Body 直连 Tip 的响应里，显示子名和稳定子名没有分清：

- `subname` 应保持当前 Body 显示路径，例如 `Pad.Face5`。
- `stableSubname` 应尽量追到源 profile，例如 `Sketch.Face1` 或 `Sketch.Edge1`。
- 当前已观察到 `Body.Face5 stableSubname` 仍是 `Pad.Face5`，测试期望 `Sketch.Face1`。

这说明 Pad / Body 的 topo source map 只保留了 Tip 自己的局部名字，缺少从草图 profile 到 Pad 拉伸结果、再到 Body direct Tip response 的稳定命名传播。

## 当前基线

- 已知 failing test 在 `cad-core/tests/test_adapters.py::CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names`。
- `rect-pad` Body response 需要同时满足：
  - `Face5.subname == "Pad.Face5"`。
  - `Face5.stableSubname == "Sketch.Face1"`。
  - `Edge3.stableSubname == "Sketch.Edge1"`。
- 当前 Pad / Body live NamedShape 只可靠发布了 `Pad.FaceN/EdgeN/VertexN` 这类 Tip-local 名字；`Sketch.Face1 -> Pad.Face5` 与部分 `Sketch.EdgeN -> Pad.EdgeN` 传播不完整。
- 这个问题独立于 open-wire `ReferenceShadow` S3；不能靠改 `ReferenceShadow` 或 adapter 输出兜底解决。

## FreeCAD 依据

普通 Pad 路径：

1. `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeaturePad.cpp::Pad::execute()`
   - 关键短句：`return buildExtrusion(ExtrudeOption::MakeFace | ExtrudeOption::MakeFuse);`
2. `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion()`
   - 关键流程：生成单侧或双侧 prism，之后 `rawShape = prism`，再 `prism = refineShapeIfActive(prism)`，最后 `AddSubShape.setValue(prism)`。
3. `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::generateSingleExtrusionSide()`
   - 普通长度、无 taper 路径调用 `prism.makeElementPrism(sketchshape, length * gp_Vec(dir));`
4. `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPrism()`
   - 关键短句：`BRepPrimAPI_MakePrism mkPrism(base.getShape(), vec);`
   - 随后 `return makeElementShape(mkPrism, base, op);`
5. `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRefine()`
   - `MyRefineMaker::populate()` 消费 `BRepBuilderAPI_RefineModel::Modified()`。
   - `mapper.init(shape, mkRefine.Shape())` 后进入 `makeShapeWithElementMap()`。

结论：正确修复路径是迁移 FreeCAD 的 maker / mapper history 语义，让 `BRepPrimAPI_MakePrism` 和可选 `RefineModel` 生成的结果面、边继续携带源 profile 的 ElementMap，而不是在 Body response 层按 FaceN 猜。

## cad-core 落点

- `cad-core/src/part_design/feature_extrude.cpp`
  - `makePrismSide()` 当前负责 Pad/Pocket 的普通 prism side build。
  - 需要确保 face profile 分支和非 face profile 分支都走同一套 maker-history source ledger，不丢失 source `NamedShape`。
- `cad-core/src/part/topo_shape.cpp`
  - `namedShapeForMakerHistory()` 是 `BRepPrimAPI_MakePrism` history 的主入口。
  - 需要补齐 source face 到 result cap face、source edge 到 result side edge/side face 的 ElementMap 传播。
  - `namedShapeForRefineHistory()` 负责 RefineModel 后续传播，不能让 refine 后的结果退回 `Pad.FaceN`。
- `cad-core/src/part_design/body.cpp`
  - `directTipSubshapeOwnerForBody()` 只决定 Body 子形显示 namespace 归属 Tip；不要在这里推断源 profile。
- `cad-core/src/runtime/recompute.cpp`
  - `bodyTipQualifiedStableSubname()` 只做 Body response 的 Tip 前缀策略；它应消费 `NamedShape.elementMap` 给出的稳定名，不应按 `Face5` 特判。

## 实施批次

### S0：隔离基线与脏工作区

当前工作区已有 Pad/Pocket/open-wire 扩展方向的未提交变更。实现本问题前先二选一：

- 在干净 worktree 从当前提交 `acda0c97df` 开新分支验证。
- 或先把无关 Pad/Pocket/P7 变更按其所属任务提交/暂存清楚。

不要把本 blocker 的修复混入 `7-1-12-21-PadPocket-open-wire拉伸扩展要求.md` 方向。

### S1：把失败断言拆成最小 topo 语义测试

新增或收紧 focused tests：

- Pad standalone NamedShape：断言 Pad 的 `element_map` 有 `Sketch.Face1 -> Face5`，以及代表性 `Sketch.Edge1 -> Edge3`。
- Body direct Tip response：断言 `subname` 是 `Pad.FaceN`，`stableSubname` 是 `Sketch.*`。
- Refine=false 与 Refine=true 各至少一例；若 Refine=true 当前存在命名顺序差异，只允许归类为命名顺序差异，不允许丢失 source stable。

### S2：修补 prism maker history

在 `namedShapeForMakerHistory()` 或其 prism 专用分支补齐：

- 对 source face 调用 `maker.Generated(sourceFace)` / `maker.Modified(sourceFace)` 时，识别 result cap face 并写入 `Sketch.Face1 -> FaceN`。
- 对 source edge，保留现有侧面传播，并补齐当前缺失的 result edge stable 传播。
- 传播时使用 `sourceElementNames(source, localElementName)`，让已有 nested source 名如 `Sketch.Face1`、`Sketch.Edge1` 自然进入 ElementMap。
- 不按 fixture 名、Face5、矩形四边形、bbox 或几何坐标写规则。

### S3：串联 RefineModel history

若 Pad `Refine=true` 或 Body replay 后进入 refine：

- 让 `namedShapeForRefineHistory()` 消费 prism 输出的 `NamedShapeSource`。
- `Modified()` 后的 result face 继承原 source stableSubname。
- `IsDeleted()` 的 source element 保留 terminal history，供后续引用更新诊断。
- 如果 refine 合并多个面，输出 merge history，不要静默选择其中一个 source。

### S4：保持 Body response 只做命名展示转换

Body 层只负责：

- `indexed` 保持 Body 自己的 `FaceN/EdgeN/VertexN`。
- `subname` 对 direct Tip 转成 `Pad.FaceN`。
- `stableSubname` 使用 `NamedShape` 已解析出的 source stableSubname；若 source stable 为空，才保留 `Pad.FaceN`。

Body / adapter 层不得新增 `Face5 -> Sketch.Face1` 之类映射表。

## 验收命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_body_direct_tip_subshapes_publish_tip_qualified_stable_names
git diff --check
```

建议新增 focused tests 后同步跑：

```bash
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

阶段回归：

```bash
python3 -m unittest tests.test_adapters.CadCoreAdapterTest
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
```

如果改到 RefineModel 或通用 `NamedShape` history，再追加：

```bash
python3 -m unittest tests.test_mvp
```

## 完成定义

1. `rect-pad` Body response 中 `Face5.subname == "Pad.Face5"`，同时 `Face5.stableSubname == "Sketch.Face1"`。
2. 代表性 edge 可从 Body direct Tip response 追到 `Sketch.EdgeN`，不是只停在 `Pad.EdgeN`。
3. Pad 自身 `NamedShape.elementMap` 能解释这些 source stableSubname，Body / adapter 没有 FaceN 特判。
4. RefineModel 路径不丢失 prism source stable；无法唯一传播时给出 split / merge / deleted history 或 diagnostics。
5. open-wire `ReferenceShadow` S3 focused tests 仍通过，证明本修复没有回退刚完成的 open-wire identity 合同。

## 禁止事项

- 不按 `rect-pad`、`Face5`、矩形草图、bbox、面数量或 OCCT 枚举顺序写特判。
- 不在 `runtime/recompute.cpp` 或 adapter 层伪造 `Sketch.Face1`。
- 不把 `subname` 改成 `Sketch.Face1`；显示路径仍必须是 `Pad.Face5`。
- 不为了让一个 adapter 断言通过而放宽测试期望。
