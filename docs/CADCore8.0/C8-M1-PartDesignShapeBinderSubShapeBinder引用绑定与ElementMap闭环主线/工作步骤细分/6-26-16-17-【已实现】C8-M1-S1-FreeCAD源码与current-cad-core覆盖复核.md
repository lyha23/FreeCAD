# 【已实现】C8-M1-S1 FreeCAD 源码与 current cad-core 覆盖复核

## 目标

复核 ShapeBinder / SubShapeBinder 的 FreeCAD source authority、上游测试证据、current `cad-core` registry / executor / topo 能力和不可跨越边界。S1 不采 oracle，不改 C++，不新增 fixture / expected / collector / tests。

## live 基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- `HEAD=16f9ff3ab1`（`16f9ff3ab1 docs: 完成 C8-M1 S0 基线冻结`）
- S1 开始时 `git -c core.quotepath=false status --short -uall` 为空，未发现无关 dirty 文件。
- 队列首项是本 S1 文件；验证通过后已按完成规则重命名为 `6-26-16-17-【已实现】C8-M1-S1-FreeCAD源码与current-cad-core覆盖复核.md`。

## FreeCAD source authority

| source | symbol | S1 结论 |
| --- | --- | --- |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:147` | `ShapeBinder::getFilteredReferences()` | 只选择第一个 `Part::Feature`；空 sub list 表示 whole shape；同一对象的非空 subvalues 被合并；没有 Part feature 时才查 `App::Line` / `App::Plane` / `App::Point` datum fallback。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:215` | `ShapeBinder::buildShapeFromReferences()` | whole shape、单 subshape、多 subshape compound、Line edge、Plane face、Point vertex 都在同一 build 分支内；ShapeBinder executor 不能退化成 display-only copy。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:100` | `ShapeBinder::updatedShape()` | 先过滤 Support 并 build shape；`TraceSupport` 为真时计算 `sourceCS`、`targetCS` 并应用 `targetCS.inverse() * sourceCS` 到 shape placement。 |
| `src/Mod/PartDesign/App/ShapeBinder.h:89` | `SubShapeBinder` properties | `Support`、`Relative`、`Fuse`、`MakeFace`、`BindMode`、`PartialLoad`、`Context`、`BindCopyOnChange`、`Refine`、`Offset*` 都是 Binder DTO / executor 边界的一部分。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:593` | `SubShapeBinder::update()` | 解析 Relative / Context / matrix cache，按 Support 取 shape；`NeedSubElement | ResolveLink | Transform`、`shapeOwners`、`shapeMats` 和 CopyOnChange temporary copy 都属于主调用链。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:858` | `SubShapeBinder::update()` geometry | FreeCAD 对外部 ElementMap 做 Shapebinder retag，随后执行 `makeElementTransform`、`makeElementCompound`、可选 fuse、`makeElementWires`、`makeElementFace`、`makeElementOffset2D`、`makeElementRefine`。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:477` | `SubShapeBinder::getSubObject()` | 先走 `Part::Feature::getSubObject()`；失败后沿 Support 子对象链解析 nested route，支持 `$Label`，并把 Binder `Placement` 乘到 transform matrix。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:529` | `setupCopyOnChange()` / `checkCopyOnChange()` | 单 Support 且 `BindCopyOnChange` 非 Disabled 时调用 `LinkBaseExtension::setupCopyOnChange()`；属性变更清 copied object cache；copy-on-change 属性分歧会把 `BindCopyOnChange` 置为 Mutated。 |
| `src/Mod/PartDesign/App/ShapeBinder.cpp:1173` | `SubShapeBinder::setLinks()` | 拒绝 invalid / cyclic reference；空 sub list 归一为 whole selection；whole selection 覆盖 child subnames；非 reset 时合并旧 Support。 |
| `src/Mod/PartDesign/App/Body.cpp:196` | `Body::isAllowed()` | Body Group 明确允许 `PartDesign::ShapeBinder` 和 `PartDesign::SubShapeBinder`，因此 Binder 输出必须能进入 Body replay。 |
| `src/Mod/PartDesign/App/Feature.cpp:336` | `Feature::getBaseShape()` / `getBaseTopoShape()` | Binder 不可作为 BaseFeature base shape 使用；下游消费要区分 Binder profile / Body Group 与 BaseFeature solid 语义。 |

## 上游测试证据

- `src/Mod/PartDesign/PartDesignTests/TestShapeBinder.py` 覆盖 two-body ShapeBinder Face support、SubShapeBinder edge offset length、Binder before / after Pad 的 bbox 和 normal 等价、SubShapeBinder 作为 Revolution profile。
- `src/Mod/PartDesign/PartDesignTests/TestTopologicalNamingProblem.py` 覆盖 ShapeBinder 作为 Revolution profile 可用，以及 `testBodySubShapeBinderElementMap()` 中 SubShapeBinder 输出 `ElementMapSize=26`、Body / BaseFeature ElementMap 差异。
- 这些测试只提供 S3 oracle 候选来源；S1 不把源码和测试存在直接提升成 supported。

## current cad-core coverage

| path | current coverage | S1 状态 |
| --- | --- | --- |
| `cad-core/src/runtime/feature_registry.cpp` | 注册了 Body、datum、FeatureBase、Boolean、DressUp、Hole、Pattern、Pad/Pocket、Pipe/Loft、Revolution/Groove、Scaled、Chamfer 等。 | 未注册 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder` 或 `PartDesign::SubShapeBinderPython`；这是 current gap evidence。 |
| `cad-core/src/part_design/body.cpp` | `getBodyTopoShapeAtFeature()` / `executeBody()` 已能按 Body.Group / Tip replay shapes、发布 Body `NamedShape`、mesh、subshapes 和 documentObjectUpdates。 | 可复用为 Binder 下游消费，不等于 Binder executor 已支持。 |
| `cad-core/src/part_design/profile_resolver.cpp` | 已有 Body-local profile context、Sketch InternalFace StableSubList、ReferenceShadow 恢复和 profile diagnostics。 | 可复用为 Binder-as-profile 消费边界，但没有创建 Binder shape。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | 已有若干 NamedShape / mapper history builder、Loft/Pipe/Revolve/FilledFace 等几何扩展能力。 | 可复用部分 topo / ElementMap 机制；尚无 ShapeBinder / SubShapeBinder 的 `makeElement*` 调用序列封装。 |
| `cad-core/src/part/property_topo_shape.cpp` | 支持 `FaceN` / `EdgeN` / `VertexN` 解析、subshape lookup 和 subshape map 输出。 | 可复用 ShapeBinder subshape lookup，不是 Binder Support parser。 |
| `cad-core/src/app/copy_on_change.cpp` | 实现 App::Link `LinkCopyOnChange` deep-copy lifecycle 的 request-local `documentObjectUpdates`。 | 可复用 CopyOnChange 设计材料，但 FreeCAD Binder 使用 `BindCopyOnChange`、temporary document 和 copied object cache，不能直接标 supported。 |
| `cad-core/src/runtime/reference_resolution.cpp` | 已能用 `ReferenceShadow` / `NamedShape` 生成 reference recovery diagnostics 和 `elementReferenceUpdates`。 | 可复用 Binder stable reference 发布路径；S1 不重开 C7-M7 imported ElementMap / ShowElement persistent writeback。 |
| `cad-core/tests/test_p7_features.py` / `test_p8_features.py` | 覆盖 Body replay、profile resolver、ElementMap history、ReferenceShadow、App::Link CopyOnChange 和 documentObjectUpdates。 | 没有 Binder-specific tests；S4/S5 之后才应新增 `test_c8_shapebinder.py` 或 focused section。 |

`rg -n 'ShapeBinder|SubShapeBinder' cad-core/src cad-core/tests` 当前只命中 `cad-core/src/sketcher/sketch_object.cpp` 中 fixture-facing alias 注释；没有 cad-core Binder executor、registry 或 tests。

## 矩阵回写

- `c8m1_shapebinder_source_authority.tsv`：已把 source row 扩展为函数/字段级证据，覆盖 ShapeBinder、SubShapeBinder、Body/Feature 下游和上游测试候选。
- `c8m1_shapebinder_scope.tsv`：每个 scope 都写明 current status；保持 `backend_gap_candidate`、`oracle_candidate` 或 `diagnostic_non_goal`，不发布 supported。
- `c8m1_shapebinder_non_goal_registry.tsv`：补充 adapter/output patch 与 C7-M7 imported ElementMap / ShowElement persistent writeback 不重开边界。
- `c8m1_shapebinder_blocker_queue.tsv`：`C8M1-BLOCKER-101` 关闭到 S1 source authority 证据，不关闭 S3/S4 oracle / implementation gate。
- `c8m1_shapebinder_validation_matrix.tsv`：补充 S1 required source rg 和 registry-gap no-match 检查。

## 下一步

进入 S2：把上述 source authority 转成 oracle candidate / backend gate 候选矩阵。S2 仍不能采 oracle、不能改 C++，也不能把 source-only evidence 升为 `backend_gap_requires_implementation`。

## 非目标

- 不采 FreeCAD oracle。
- 不新增 fixture / expected / tests / collector。
- 不修改 C++ 或 `cad-core/src`。
- 不把 source-only evidence 提升为 supported 或 `backend_gap_requires_implementation`。
- 不重开 C7-M7 imported ElementMap / ShowElement persistent writeback。
- 不做 adapter/output 层补丁。
