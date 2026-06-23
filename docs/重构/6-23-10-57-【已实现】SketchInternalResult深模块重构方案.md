# Sketch Internal Result 深模块重构方案

> 实现状态：已实现。当前核心代码已新增 `sketch_internal_result` deep module，`executeSketchObject()` 已只保留输入解析、raw sketch 构造和 result package 合并；InternalShape 的 `ShapeValue`、`NamedShape`、mesh/subshape map、internal element map、FaceMaker/WireJoiner debug projection 已统一迁入新模块。已用 `cmake --build build`、P5 sketch 回归、adapter 回归与 `git diff --check` 验证。

## 来源与结论

本方案来自 `/var/folders/5k/fms98vy54k18w9n0j5_53r400000gn/T/architecture-review-20260623-154500.html` 中的第二个 Strong 候选项 `Collapse Sketch Internal Result`。第一个 Strong 候选项 `ProfileBased Profile` 已完成并更名为已实现方案，因此下一步优先处理 Sketch Internal Result。

当前判断：`cad-core` 已经有 `sketch_internal_builder`，但它只负责构造 `profileShape`、`internalShape` 和 FaceMaker / WireJoiner summary；真正复杂的结果装配仍留在 `executeSketchObject()` 里。这个 module 还不够 deep，interface 没有把复杂度真正收进去。

## 当前基线

当前 `cad-core/src/sketcher/sketch_object.cpp::executeSketchObject()` 仍同时承担多类职责：

- 读取 Sketch `Geometry` / `Constraints` / `Support` / `SketchPlaneFrame` / external geometry。
- 构造 raw sketch `Shape`、profile edges、profile face 和 `InternalShape`。
- 对 raw/profile/internal shape 应用 placement。
- 构造 `runtime::ShapeValue`，填充 `profileShape`、`profileNormal`、`internalShape`、`profileRequiresSubshapeSelection`。
- 把 FaceMaker / WireJoiner summary 逐字段翻译成 `part::SketchInternalHistoryContext`。
- 生成 `InternalShape` 的 `NamedShape` / `ElementMap`，写入 `context.namedShapes[Sketch.InternalShape]`。
- 生成 request-local mesh、raw + internal subshape map、internal element map。
- 在 `context.objects[Sketch]` 里混合写入 solver 状态、external geometry 状态、internal shape 统计、FaceMaker/WireJoiner debug ledger。

现有 `cad-core/include/cad_core/sketcher/sketch_internal_builder.h` 与 `cad-core/src/sketcher/sketch_internal_builder.cpp` 已经是一个好的起点，但它只覆盖 FreeCAD `SketchObject::buildInternals()` 的几何构造部分。结果发布、命名账本和 debug projection 仍然散在 caller glue 中。

## FreeCAD 依据

FreeCAD 的语义入口集中在 `SketchObject`：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::execute()`：先写 `InternalShape.setValue(buildInternals(...))`，再写 `Shape.setValue(result)`，因为引用可能指向 `InternalShape`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::buildInternals()`：调用 `Part::FaceMakerBuildFace`，再用 `Part::WireJoiner` 追加 open wires，最后在 face result 与 open wires 都存在时 `makeElementCompound({result, openWires})`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::getInternalElementMap()`：只把 `InternalVertex` / `InternalEdge` 与 raw `Vertex` / `Edge` 做共享顶点映射；`InternalFace` 的稳定语义依赖 FaceMaker / WireJoiner history。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObject.cpp::SketchObject::getElementTypes()`：公开 `InternalEdge`、`InternalFace`、`InternalVertex`。
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/FaceMaker.cpp::FaceMaker::postBuild()` 与 `/Users/li/Chili3DProject/FreeCAD/src/Mod/Part/App/WireJoiner.cpp::WireJoinerP::getOpenWires()`：提供后续 `ElementMap` 消费所需的 history / MapperHistory 依据。

因此 `cad-core` 的重构方向不是把 Sketch solver、FaceMaker、WireJoiner 或 TopoShape 合并成一个大文件，而是把 FreeCAD `SketchObject` 的 request-local internal result 发布语义集中成一个 deep module。

## 目标 module

新增或深化 `sketcher/Sketch Internal Result` module。

建议落点：

- `cad-core/include/cad_core/sketcher/sketch_internal_result.h`
- `cad-core/src/sketcher/sketch_internal_result.cpp`
- 保留并复用 `cad-core/include/cad_core/sketcher/sketch_internal_builder.h`
- 保留并复用 `cad-core/src/sketcher/sketch_internal_builder.cpp`

这个 module 应承接：

- 调用或消费 `buildSketchInternals()` 的结果。
- 统一处理 raw/profile/internal shape 的 placement 后结果。
- 生成 `runtime::ShapeValue` 的 Sketch-specific 字段。
- 生成 `Sketch.InternalShape` 的 `NamedShape` 与 `SketchInternalHistoryContext`。
- 生成 request-local mesh 与 raw/internal subshape map。
- 生成 internal element map、internal shape count、FaceMaker/WireJoiner debug ledger 的 typed projection。
- 返回一个可由 `executeSketchObject()` 直接写入 `ComputeContext` 的 result package。

重构后的 `executeSketchObject()` 角色：

- 保留输入校验、Geometry/Constraint/Support/ExternalGeometry 解析和 solver-facing 状态。
- 保留 raw sketch geometry 构造入口。
- 调用 `Sketch Internal Result` module。
- 把 module 返回的 `ShapeValue`、mesh、subshapes、named shape、internal debug fields 合并进 `ComputeContext`。
- 不再逐字段翻译 FaceMaker/WireJoiner history，不再直接拼 InternalShape subshape map，不再把 internal result 的 debug ledger 展开在 executor 主体中。

## 分层落点

- `sketch_object.cpp`：删除 internal result 组装细节，只保留 SketchObject 调用顺序和 solver/external geometry 状态。
- `sketch_internal_builder.*`：继续只负责 `buildInternals()` 几何路径，避免把 JSON 或 runtime context 写入 builder。
- `sketch_internal_result.*`：新增 deep module，承接 runtime-facing result package、history context conversion 和 internal debug projection。
- `part/topo_shape.*`：继续负责 `NamedShape` / `ElementMap` / history 消费，不倒灌 Sketch executor 业务流程。
- `runtime/recompute.cpp`：继续负责全局 recompute 和 response serialization，不接收 Sketch Internal Result 的内部字段逐项拼装职责。

## 实施步骤

### S0：冻结当前行为面

先不改代码，只列出本轮必须保持稳定的行为：

- raw Sketch `Shape` 仍可用于 open profile / external geometry / solver-facing 输出。
- `InternalShape` 空、缺失、非空三种状态保持不变。
- `InternalFaceN`、`InternalEdgeN`、`InternalVertexN` 的 mesh faceIds/subshape id 不漂移。
- `Sketch.InternalShape` 的 `NamedShape`、`ElementMap`、FaceMaker/WireJoiner history debug 信息不丢失。
- `Profile.SubList=InternalFaceN` 与 ProfileBased resolver 的已实现行为不回退。
- Adapter 输出里的 `InternalFace` subshapes 和 `elementReferenceUpdates` 不改变 JSON contract。

本步建议只形成一份影响清单，不新增大型矩阵。

### S1：定义 SketchInternalResult package

新增 typed result，而不是让 caller 继续搬运散字段：

- `runtime::ShapeValue shapeValue`。
- optional `part::NamedShape internalNamedShape`。
- optional mesh JSON。
- subshape map JSON。
- internal result object fields JSON。
- FaceMaker/WireJoiner debug fields JSON。
- diagnostics / warning flags，例如 face maker failed、requires subshape selection。

新增 public 类型必须在相邻注释中标注 FreeCAD 依据，至少指向 `SketchObject::buildInternals()`、`getInternalElementMap()` 和 `getElementTypes()`。

### S2：迁移 history context conversion

把 `executeSketchObject()` 中 FaceMaker / WireJoiner summary 到 `part::SketchInternalHistoryContext` 的大段逐字段转换迁入 `sketch_internal_result.cpp`。

迁移后要求：

- `executeSketchObject()` 不再出现 `SketchInternalWireJoiner*` / `SketchInternalFaceMaker*` 的逐字段赋值。
- `topo_shape` 仍只消费 typed `SketchInternalHistoryContext`，不直接依赖 Sketch executor。
- 不删减任何现有 debug ledger 字段；第一轮只移动，不改语义。

### S3：迁移 InternalShape publication

把以下发布逻辑收进 `Sketch Internal Result` module：

- `shapeValue.internalNamedShape = namedShapeForSketchInternalShape(...)`。
- `context.namedShapes[object.name + ".InternalShape"]` 所需数据。
- `meshForShape(*internalShape, "InternalFace", "InternalEdge", "InternalVertex")`。
- raw subshape map 与 internal subshape map merge。
- `app::internalElementMapForSketch(*rawShape, *internalShape)`。
- internal face/edge/vertex count。

迁移时不要让 module 直接持有整个 `ComputeContext` 作为长期 dependency；优先让 module 返回 result package，再由 executor 写 context。只有 diagnostics 这种必须追加的内容可以通过明确参数传入。

### S4：拆分 object JSON projection

把 `context.objects[Sketch]` 拆成两部分：

- caller 保留 solver / constraint / external geometry / high-level status fields。
- `Sketch Internal Result` module 返回 internal result fields，包括 internal shape 状态、internal element map、FaceMaker/WireJoiner ledger summary。

最终由 `executeSketchObject()` 做一次 shallow merge。这样 debug JSON 仍保持兼容，但新增 internal ledger 字段不用继续扩大 executor。

### S5：补 focused test surface

优先补或整理能直接约束 module 输出的测试：

- InternalShape 非空时，mesh faceIds 包含 `Sketch:InternalFaceN`。
- raw + internal subshape map 同时存在时，raw `EdgeN` 与 internal `InternalEdgeN` 不互相覆盖。
- open-only sketch 不产生错误的 profile face，但 raw shape 仍存在。
- FaceMaker/WireJoiner history debug 字段在迁移前后保持一致。
- `Profile.SubList=InternalFaceN` 仍能通过 ProfileBased resolver 消费。

若 C++ test target 成本过高，第一轮可先用 Python fixture 保护行为；但不要只靠全量 CLI golden output，至少要有 focused assertions。

## 非目标

- 不实现完整 Sketcher solver。
- 不改 `ProfileBased Profile` resolver 的接口和行为。
- 不迁移 FaceMaker / WireJoiner 内部算法。
- 不改变 `NamedShape` / `ElementMap` 的拓扑命名规则。
- 不改变 runtime response JSON contract。
- 不删除现有 debug ledger 字段。
- 不把 `sketch_object_operations`、external geometry、support placement 全部合并进新 module。

## 风险与控制

- 风险：迁移 debug JSON 时字段名或空值行为漂移。控制：S2/S4 只移动字段，不重命名；用 focused tests 对关键字段断言。
- 风险：new module 变成第二个 `executeSketchObject()`。控制：只收 internal result publication，不收 geometry parse、constraint solver 和 external geometry。
- 风险：把 `ComputeContext` 整体传进 module 后形成新的隐式依赖。控制：module 返回 result package，由 executor 统一写 context。
- 风险：移动 FaceMaker/WireJoiner history conversion 时遗漏某个 ledger 字段。控制：迁移前后用 `rg` 检查 `wire_joiner_ledger`、`FaceMaker`、`WireJoiner` 相关字段，必要时先做字符串字段清单。
- 风险：借重构顺手修 topology parity，导致回归范围扩大。控制：本轮只做结构迁移；发现语义缺口另开实现方案。

## 验收命令

### 本轮短跑

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest
python3 -m unittest tests/test_adapters.py
git diff --check
```

若只完成 S1/S2，可先跑更窄的 InternalFace / WireJoiner / adapter focused filter，再补一次上述短跑。

### 阶段回归

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_feature_flows.py tests/test_diagnostics.py tests/test_p6_topology.py tests/test_p7_features.py
python3 -m unittest tests/test_adapters.py
```

### 重型收口

仅在迁移完成并触碰 `NamedShape` / `ElementMap` / adapter response projection 时执行：

```bash
cd ~/Chili3DProject/FreeCAD/cad-core
python3 -m unittest discover -s tests
```

## 推荐顺序

优先执行 S0-S4。理由是当前最大 friction 已经不在 `sketch_internal_builder` 的几何构造，而在 `executeSketchObject()` 对 internal result 的发布和 debug ledger 展开。先把这一段收进 `Sketch Internal Result` module，可以提升 locality，并让后续 FaceMaker/WireJoiner/ElementMap parity 工作有一个更稳定的 test surface。
