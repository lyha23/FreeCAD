# 【已实现】C5-M4 Datum Attachment 引用稳定方案

## 目标

C4 已把 active `AttachmentSupport` / `MapMode` 作为稳定 unsupported diagnostic 收口。C5-M4 的目标是判断哪些非 GUI AttachEngine map modes 对前端 CAD runtime 有价值，并把这些模式做成 expected-backed support；其余继续保持 locatable diagnostic。

## 范围

- Datum 源码依据：`src/Mod/PartDesign/App/DatumPoint.cpp`、`DatumLine.cpp`、`DatumPlane.cpp`、`DatumCS.cpp`。
- Attachment 源码依据：`src/Mod/Part/App/AttachExtension.cpp`、`AttachExtension.h`、`Attacher.cpp`、`Attacher.h`。
- link 依据：`src/App/PropertyLinks.cpp`。
- cad-core 落点：`cad-core/src/part_design/datum_*`、`cad-core/src/app`、`cad-core/src/runtime/recompute.cpp`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p7_features`、`tests.test_adapters`；若采集 native expected，再加 `tests.test_expected_fixtures`。

## 产品 gate

M4 不是默认迁移完整 AttachEngine。进入 support 的 map mode 必须同时满足：

- 不依赖 GUI Attachment editor。
- 可由 request graph 的 `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`Reverse`、`Parameter` 完整描述。
- 能用 native expected 或稳定 placement / downstream reference assertion 验收。
- 不引入跨请求 attachment session。

## FreeCAD 调用链记录

- Datum 构造差异：`DatumPoint.cpp::Point::Point()` 调用 `setAttacher(new AttachEnginePoint)`；`DatumLine.cpp::Line::Line()` 调用 `setAttacher(new AttachEngineLine)` 并创建无限 Z 轴 edge；`DatumPlane.cpp::Plane::Plane()` 调用 `setAttacher(new AttachEnginePlane)` 并创建无限 XY face；`DatumCS.cpp::CoordinateSystem::CoordinateSystem()` 调用 `setAttacher(new AttachEngine3D)`，`getXAxis/getYAxis/getZAxis()` 都从 `Placement` 旋转取轴。
- AttachExtension 属性模型：`AttachExtension.cpp::AttachExtension()` 声明 `AttachmentSupport`、`MapMode`、`MapReversed`、`MapPathParameter`、`AttachmentOffset`，`MapMode` 默认 `mmDeactivated`，并把 `MapMode` enum 绑定到 `AttachEngine::eMapModeStrings`。
- 执行链：`AttachExtension::extensionExecute()` 在 mapping touched 时调用 `positionBySupport()`；`positionBySupport()` 先 `updateAttacherVals()`，再清空当前 `Placement`，处理 base attacher，随后执行 `setOffset(AttachmentOffset.getValue() * basePlacement.inverse())` 和 `calculateAttachedPlacement(plaOriginal, &subChanged)`；若 `subChanged`，会把 attacher 规范化后的 subnames 写回 `AttachmentSupport.setValues(...)`。
- AttachEngine 求解链：`Attacher.cpp::AttachEngine::setUp()` 保存 references、`mapMode`、`mapReverse`、`attachParameter` 和 offset；`calculateAttachedPlacement()` 先用 `shadowSubs` / `Feature::getRelatedElements(..., followTypeChange)` 尝试恢复缺失 subname，只有当新旧 placement 在容差内一致时才允许写回 subname；最后进入各 engine 的 `_calculateAttachedPlacement()`。
- mode 分类依据：`AttachEngine3D` 承接 `ObjectXY/ObjectXZ/ObjectYZ/FlatFace/TangentPlane/NormalToEdge/Frenet*/Concentric/SectionOfRevolution/ThreePoints*/Folding/InertialCS/OZX..OYX/ParallelPlane/MidPoint` 等；`AttachEnginePlane` 复用 3D；`AttachEngineLine` 把 `ObjectX/ObjectY/ObjectZ/AxisOfCurvature/Tangent/Normal/Binormal/FaceNormal` 等映射或专门求解；`AttachEnginePoint` 把 `ObjectOrigin/OnEdge/CenterOfCurvature` 等映射或专门求解。
- downstream reference 稳定：`src/App/PropertyLinks.cpp::PropertyLinkBase::updateElementReferences()`、`_updateElementReference()`、`PropertyLinkSubList::setValues()` 和 `onContainerRestored()` 维护 `ShadowSub` / old-new element names；M4 当前只在 cad-core request model 中读取 `ShadowSub` / `ReferenceShadow` 证据，尚无 AttachEngine placement 等价求解和 subname 写回闭环。

## C5-S4 gate 结论

本轮不发布 selected map mode support。原因是即使 `FlatFace` / `ObjectXY` 这类表面上可直接映射的模式，也会经过 `AttachmentOffset`、`MapReversed`、`MapPathParameter`、engine type 映射和 `shadowSubs` 下游引用恢复；当前 cad-core 没有等价的 request-local AttachEngine 和 `AttachmentSupport` subname 写回，所以 native expected 的单一 placement fixture 不能证明 downstream reference stability。

分类如下：

| 分类 | mode / 字段 | 状态 |
| --- | --- | --- |
| supported existing | 无 active `AttachmentSupport` / `MapMode` 的 DatumPoint/Line/Plane/CoordinateSystem placement、Body Origin datum role relink、下游 ReferenceAxis 使用已存在 DatumLine/DatumCS | 保持 supported |
| diagnostic-backed | active `AttachmentSupport` 但 `MapMode=Deactivated` | `AttachmentSupport` locatable diagnostic |
| diagnostic-backed | `FlatFace`、`ObjectXY/ObjectXZ/ObjectYZ`、`ObjectOrigin/ObjectX/ObjectY/ObjectZ`、`NormalToEdge` 等 selected 候选 | `MapMode` locatable diagnostic，不发布 support |
| diagnostic-backed | `AttachmentOffset`、`MapReversed` / Reverse、`MapPathParameter` / Parameter | 单独字段 diagnostic，target/subname 跟随 support |
| diagnostic-backed | `StableSubList` / `ShadowSub` 证据和 downstream ReferenceAxis 依赖 attached Datum | attached Datum 保持 unsupported diagnostic；依赖对象稳定跳过，不能自动写回引用 |
| deferred | full AttachEngine mode list、Point/Line/Plane/3D 专用求解、inertia/proximity/tangent/frenet/folding/parallel/midpoint 等 | 后续 AttachEngine 专题 |
| non-goal | GUI Attachment editor、ViewProvider resize、task panel、跨请求 attachment session | 不进入 cad-core |

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | source audit：AttachExtension property、AttachEngine map-mode list、Datum engine type |
| S1 | product gate：选择 FlatFace / NormalToEdge / Inertial 等候选或保留 diagnostic |
| S2 | expected fixture / placement / downstream reference tests |
| S3 | capability metadata 和 remaining boundary 收口 |

## 非目标

- 不迁移 GUI Attachment editor、ViewProvider resize 或 task panel。
- 不支持完整 AttachEngine mode list，除非产品 owner 逐项接受。
- 不保存跨请求 attachment session。

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
python3 -m unittest tests.test_p7_features tests.test_adapters
```

## C5-S4 产物

- 新增 `cad-core/fixtures/c5m4/partdesign-datum-attachment-mapmode-diagnostics.json`：覆盖 `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed`、`MapPathParameter`、target/subname、`ShadowSub` 证据和下游 `ReferenceAxis` 依赖 attached Datum 的稳定 diagnostic。
- 更新 `cad-core/src/part_design/datum_attachment.h` 与四个 Datum executor：识别 AttachExtension 字段，但 active attachment 仍统一落成 locatable unsupported diagnostic。
- 更新 `cad-core/src/adapters/c_api/c_api.cpp` capability：保持 existing placement/link slice supported，明确 selected map mode、offset/reverse/parameter 和 shadow-sub writeback 为 deferred。
