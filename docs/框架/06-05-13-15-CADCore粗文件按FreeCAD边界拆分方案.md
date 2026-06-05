# CAD Core 粗文件按 FreeCAD 源码边界拆分方案

本文只处理 `cad-core/src` 中若干粗文件的后续结构治理。目标不是为了“小文件化”降低行数，而是让文件边界更接近本地 FreeCAD 源码，减少后续迁移时“一个 cad-core 文件混合多个 FreeCAD 编译单元”的定位成本。

本地 FreeCAD 依据统一使用 `/Users/li/Chili3DProject/重构Chili/FreeCAD/src`。当前 include/namespace 兼容层移除工作不应被本方案扩大成行为重写。

## 当前基线

2026-06-05 已完成本轮同构拆分，范围只限 FreeCAD 本身已经按功能拆开的文件族：

- `cad-core/src/part_design/feature_dress_up.cpp` 已按 FreeCAD 的 `FeatureDressUp.cpp`、`FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDraft.cpp`、`FeatureThickness.cpp` 拆开。
- `cad-core/src/part_design/feature_transformed.cpp` 已按 FreeCAD 的 `FeatureTransformed.cpp`、`FeatureMirrored.cpp`、`FeatureLinearPattern.cpp`、`FeaturePolarPattern.cpp`、`FeatureMultiTransform.cpp`、`FeatureScaled.cpp` 拆开。
- `cad-core/src/part/part_feature.cpp` 已按 FreeCAD Part 模块的 primitive、import、offset、extrusion 源文件边界拆开；既有 `part_boolean.cpp`、`extrusion_helper.cpp`、`shape_exporter.cpp` 边界保持不变。
- 新增的 `feature_dress_up_support.h`、`feature_transformed_support.h`、`part_feature_support.{h,cpp}` 都是对应文件族内部 support，不是 public include facade，也没有落入 `runtime`、`graph` 或 `adapters`。

继续不拆：

- `cad-core/src/part/wire_joiner.cpp`：FreeCAD 也是单个 `src/Mod/Part/App/WireJoiner.cpp`，它是状态机账本文件，大并不代表边界错误。
- `cad-core/src/part_design/feature_hole.cpp`：FreeCAD 基本也是单个 `src/Mod/PartDesign/App/FeatureHole.cpp`，可后续抽资源解析 / thread / tool helper，但不作为本轮同构拆分重点。
- `cad-core/src/app/link.cpp`：FreeCAD 的 `src/App/Link.cpp` 本身也接近同等规模，另有更大的 `PropertyLinks.cpp`。Link 后续可按 FreeCAD Link 生命周期拆，但不应仅因 3000 行而硬拆。

## FreeCAD 对照基线

| cad-core 当前文件 | 当前行数 | FreeCAD 对照 | 当前状态 |
| --- | ---: | --- | --- |
| `src/part_design/feature_dress_up.cpp` + `feature_fillet.cpp` / `feature_chamfer.cpp` / `feature_draft.cpp` / `feature_thickness.cpp` | 1114 + 143 / 280 / 550 / 441 | `FeatureDressUp.cpp`、`FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDraft.cpp`、`FeatureThickness.cpp` | 已拆；shared base/source/AddSub cache 留在 DressUp support 边界 |
| `src/part_design/feature_transformed.cpp` + `feature_mirrored.cpp` / `feature_linear_pattern.cpp` / `feature_polar_pattern.cpp` / `feature_multi_transform.cpp` / `feature_scaled.cpp` | 1498 + 215 / 264 / 220 / 385 / 184 | `FeatureTransformed.cpp`、`FeatureMirrored.cpp`、`FeatureLinearPattern.cpp`、`FeaturePolarPattern.cpp`、`FeatureMultiTransform.cpp`、`FeatureScaled.cpp` | 已拆；TransformSource、copy/fuse/cut/apply 留在 transformed support 边界 |
| `src/part/part_feature.cpp` + `primitive_feature.cpp` / `part_extrusion.cpp` / `part_import.cpp` / `part_offset.cpp` | 75 + 1395 / 784 / 296 / 179 | `PartFeature.cpp`、`PrimitiveFeature.cpp`、`FeatureExtrusion.cpp`、`FeaturePartImport*.cpp`、`FeatureOffset.cpp` | 已拆；Part 基础入口留在 `part_feature.cpp`，通用发布/source link 放在 `part_feature_support.{h,cpp}` |
| `src/app/link.cpp` | 3053 | `src/App/Link.cpp` 约 2821 行，`src/App/PropertyLinks.cpp` 约 6022 行 | 本轮只评估，不拆 |
| `src/part_design/feature_hole.cpp` | 3111 | `src/Mod/PartDesign/App/FeatureHole.cpp` 约 2714 行 | FreeCAD 基本单文件，本轮不拆 |
| `src/part/wire_joiner.cpp` | 6332 | `src/Mod/Part/App/WireJoiner.cpp` 约 3217 行 | FreeCAD 单文件状态机，不拆 |

## 拆分原则

1. 只做文件边界迁移时，不改 FreeCAD 业务语义，不改 fixture expected，不改拓扑命名策略。
2. 每个新 `.cpp` / `.h` 的名字优先对应 FreeCAD 源文件名，而不是按内部 helper 随意命名。
3. 公共 executor 声明继续放在 `include/cad_core/<module>/...`，实现按 `src/<module>/...` 拆；不要恢复旧 `features/geometry/topo` facade。
4. 共享 helper 可以留在基类文件或新增 `*_support.cpp` / `*_utils.cpp`，但必须服务明确的 FreeCAD 文件族，不形成新的通用杂物目录。
5. `WireJoiner`、FaceMaker、`TopoShapeExpansion`、MapperHistory 这类内部账本文件不按行数拆散。
6. 每一轮拆分都要能用最小相关测试证明“行为未变”。

## 目标文件边界

### PartDesign DressUp 家族

当前：

```text
cad-core/src/part_design/feature_dress_up.cpp
```

建议目标：

```text
cad-core/src/part_design/feature_dress_up.cpp      # DressUpBase、AddSubShape cache、通用 base/support 解析
cad-core/src/part_design/feature_fillet.cpp        # executeFillet、fillet edge selection、OCCT fillet maker
cad-core/src/part_design/feature_chamfer.cpp       # executeChamfer、chamfer edge/face selection、OCCT chamfer maker
cad-core/src/part_design/feature_draft.cpp         # executeDraft、neutral plane、pull direction、draft builder
cad-core/src/part_design/feature_thickness.cpp     # executeThickness、face selection、thickness mode/join
```

对应 public header 已经基本具备：

```text
include/cad_core/part_design/feature_fillet.h
include/cad_core/part_design/feature_chamfer.h
include/cad_core/part_design/feature_draft.h
include/cad_core/part_design/feature_thickness.h
```

实施要点：

- 第一轮只移动 `executeFillet` / `executeChamfer` / `executeDraft` / `executeThickness` 及其直接私有 helper。
- 共享 `DressUpBase`、`resolveDressUpBase()`、`baseTopoShapeForFeature()`、`namedShapeForObject()` 等先保留在 `feature_dress_up.cpp`，必要时通过内部头 `feature_dress_up_support.h` 暴露给子文件。
- 不改变 `SupportTransform`、slot 级 `NamedShape`、链式 DressUp support 的已有行为。

### PartDesign Transformed 家族

当前：

```text
cad-core/src/part_design/feature_transformed.cpp
```

建议目标：

```text
cad-core/src/part_design/feature_transformed.cpp        # TransformSource、support source、通用 apply/copy/fuse/cut
cad-core/src/part_design/feature_mirrored.cpp           # executeMirrored、mirror plane resolution
cad-core/src/part_design/feature_linear_pattern.cpp     # executeLinearPattern、linear steps / direction
cad-core/src/part_design/feature_polar_pattern.cpp      # executePolarPattern、axis / angle / spacing
cad-core/src/part_design/feature_multi_transform.cpp    # executeMultiTransform、template transform chain
cad-core/src/part_design/feature_scaled.cpp             # executeScaled、scale center / factor
```

对应 FreeCAD 源文件：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureTransformed.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMirrored.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureLinearPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeaturePolarPattern.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureMultiTransform.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureScaled.cpp`

实施要点：

- `TransformSource`、`TransformApplication`、`TransformedBuild`、`solidSource()`、`applyFeatureTransforms()`、`applyWholeShapeTransforms()` 先作为 shared support 保留。
- 各子文件只承接自身属性读取、axis / plane / transform list 构造和 executor 入口。
- 不改变 Features / Whole shape、Body prefix support、AddSubShape slot、transformed source alias 和 terminal history 传播。

### Part Feature 家族

当前：

```text
cad-core/src/part/part_feature.cpp
```

FreeCAD 里 Part 模块拆得更细，尤其是 primitive、import、offset、extrusion、boolean 不在一个实现文件里。

建议目标：

```text
cad-core/src/part/part_feature.cpp          # Part::Feature 基础入口、通用 source link / shape resolution
cad-core/src/part/primitive_feature.cpp     # Box/Cylinder/Prism/RegularPolygon/Sphere/Ellipsoid/Cone/Torus/Wedge/Vertex/Line/Plane/Ellipse/Helix/Spiral
cad-core/src/part/part_extrusion.cpp        # Part::Extrusion source / direction / FaceMakerClass / solid source
cad-core/src/part/part_import.cpp           # ImportBrep / ImportStep / ImportIges
cad-core/src/part/part_offset.cpp           # Part::Offset
```

已有文件：

```text
cad-core/src/part/part_boolean.cpp
cad-core/src/part/extrusion_helper.cpp
cad-core/src/part/shape_exporter.cpp
```

处理建议：

- `part_boolean.cpp` 已独立，先不并回或重命名。
- `extrusion_helper.cpp` 是 Part / PartDesign 低层 helper，不等同于 `Part::Extrusion` executor；`Part::Extrusion` executor 可拆到 `part_extrusion.cpp`。
- Import / Export 要分开：Import 是 Part feature recompute，Export 是 adapter 触发的文件导出 helper。

### App Link

当前：

```text
cad-core/src/app/link.cpp
```

FreeCAD 对照：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/Link.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/App/PropertyLinks.cpp`

判断：

- 当前 `cad-core` 的 Link 文件大，但 FreeCAD `Link.cpp` 本身也大；不应作为第一轮拆分目标。
- 后续如果拆，应按 FreeCAD Link lifecycle，而不是按 helper 行数切片。

建议后续边界：

```text
cad-core/src/app/link.cpp                  # App::Link / LinkBaseExtension 主 executor
cad-core/src/app/link_element.cpp          # LinkElement、ElementCount、ShowElement materialized child
cad-core/src/app/link_group.cpp            # LinkGroup、DocumentObjectGroup plain group 展开
cad-core/src/app/link_subobject.cpp        # LinkSub path、label / object prefix 路由、subshape resolution
```

拆分前置条件：

- 当前 Link fixture 已有足够 expected 覆盖。
- 不改变 `documentObjectUpdates` 建议语义。
- 不改变 `FullSubList`、`ShadowSub`、`ReferenceShadow`、mapped alias retag 和 terminal / merge history 传播。

### Hole

当前：

```text
cad-core/src/part_design/feature_hole.cpp
```

FreeCAD 对照：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/App/FeatureHole.cpp`
- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/PartDesign/Resources/Hole/*.json`

判断：

- FreeCAD 本身基本是单个 `FeatureHole.cpp`，所以 CAD Core 当前单文件不算偏离 FreeCAD 边界。
- 可后续抽内部 support，但不作为“按 FreeCAD 源文件边界拆分”的主目标。

允许的后续抽取：

```text
cad-core/src/part_design/feature_hole.cpp             # executeHole、buildHole 主流程
cad-core/src/part_design/feature_hole_resources.cpp   # Hole cut definition / resource JSON 表
cad-core/src/part_design/feature_hole_thread.cpp      # thread diameter / clearance / model thread profile
cad-core/src/part_design/feature_hole_tool.cpp        # cylinder / counterbore / countersink / thread tool 构造
```

这些是内部复杂度抽取，不是 FreeCAD 源文件一对一拆分，优先级低于 DressUp / Transformed / Part Feature。

### WireJoiner

当前：

```text
cad-core/src/part/wire_joiner.cpp
```

FreeCAD 对照：

- `/Users/li/Chili3DProject/重构Chili/FreeCAD/src/Mod/Part/App/WireJoiner.cpp`

判断：

- 不拆。
- 该文件承载 `EdgeInfo`、`WireInfo`、`wireInfo/wireInfo2`、`iteration/iteration2`、`superEdge`、open wire export、MapperHistory 消费等状态机账本。
- 拆散账本会增加 ownership / history drift 风险，不符合 FreeCAD parity 目标。

## 推荐实施顺序

### M1：拆 DressUp 家族

目标：

- 新增 `feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_draft.cpp`、`feature_thickness.cpp`。
- `feature_dress_up.cpp` 收敛为 shared support + DressUp 基类语义。
- 更新 `cad-core/CMakeLists.txt` source list。

状态：

- 已完成。新增 `feature_fillet.cpp`、`feature_chamfer.cpp`、`feature_draft.cpp`、`feature_thickness.cpp` 和内部 `feature_dress_up_support.h`。
- `feature_dress_up.cpp` 保留 DressUp base/source 解析、AddSubShape cache、refine 和结果发布 support。

验收：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_feature_flows
cd ..
git diff --check -- cad-core docs/框架
cd cad-core && graphify update .
```

结果：构建通过；`tests.test_p7_features tests.test_feature_flows` 106 个测试通过；`git diff --check -- cad-core docs/框架` 通过；`graphify update .` 完成。

### M2：拆 Transformed 家族

目标：

- 新增 `feature_mirrored.cpp`、`feature_linear_pattern.cpp`、`feature_polar_pattern.cpp`、`feature_multi_transform.cpp`、`feature_scaled.cpp`。
- `feature_transformed.cpp` 收敛为 transformed shared support。
- 更新 CMake source list。

状态：

- 已完成。新增 `feature_mirrored.cpp`、`feature_linear_pattern.cpp`、`feature_polar_pattern.cpp`、`feature_multi_transform.cpp`、`feature_scaled.cpp` 和内部 `feature_transformed_support.h`。
- `feature_transformed.cpp` 保留 TransformSource、support source、copy/fuse/cut/apply、结果发布和 refine support。

验收：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_feature_flows tests.test_adapters
cd ..
git diff --check -- cad-core docs/框架
cd cad-core && graphify update .
```

结果：构建通过；`tests.test_p7_features tests.test_feature_flows tests.test_adapters` 123 个测试通过；`git diff --check -- cad-core docs/框架` 通过；`graphify update .` 完成。

### M3：拆 Part Feature 家族

目标：

- 新增 `primitive_feature.cpp`、`part_extrusion.cpp`、`part_import.cpp`、`part_offset.cpp`。
- `part_feature.cpp` 只保留 Part::Feature 通用入口和 shared source resolution。
- 保持 `part_boolean.cpp`、`shape_exporter.cpp`、`extrusion_helper.cpp` 的既有边界。

状态：

- 已完成。新增 `primitive_feature.cpp`、`part_extrusion.cpp`、`part_import.cpp`、`part_offset.cpp` 和内部 `part_feature_support.{h,cpp}`。
- `part_feature.cpp` 只保留 App::Part / Part::Feature 基础入口；primitive、Part::Extrusion、Part::Offset、Part Import executor 已拆到对应文件。
- `part_boolean.cpp`、`extrusion_helper.cpp`、`shape_exporter.cpp` 没有并回或改名。

验收：

```bash
cd /Users/li/Chili3DProject/重构Chili/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
cd ..
git diff --check -- cad-core docs/框架
cd cad-core && graphify update .
```

结果：构建通过；`tests.test_p8_features tests.test_adapters` 99 个测试通过；`git diff --check -- cad-core docs/框架` 通过；`graphify update .` 完成。

### M4：评估 Link 是否需要拆

目标：

- 只做评估，不默认动代码。
- 如果要拆，先列出 Link / LinkElement / LinkGroup / LinkSub path 的 fixture 覆盖矩阵。

前置：

- P8 Link fixture 对 `ShowElement`、ElementList、ElementCount、plain group、FullSubList、XLink、ReferenceShadow 更新建议有足够覆盖。
- 文档明确“不改变 documentObjectUpdates 行为”。

### M5：Hole 内部复杂度整理

目标：

- 仅在 Hole 后续行为迁移继续扩张时，把 resource / thread / tool builder 抽成内部 support 文件。
- 不把 Hole 拆成多个 FreeCAD 对应类，因为 FreeCAD 本身没有这样拆。

## 非目标

- 不在本方案里拆 `wire_joiner.cpp`。
- 不为了减少行数拆 `feature_hole.cpp`。
- 不为了小文件化拆散 FaceMaker、TopoShapeExpansion、MapperHistory 或任何 history ledger。
- 不改变 executor 行为、OCCT 构造顺序、NamedShape / ElementMap 传播、fixture expected。
- 不恢复旧 `features/geometry/topo/document` include 或 namespace。
- 不把 shared support 放到 `runtime`、`graph`、`adapters`。

## 完成标准

- 新文件能直接映射到 FreeCAD 源文件族。
- `cad-core/CMakeLists.txt` source list 与新文件同步。
- public header 仍只暴露 FreeCAD 同构模块路径。
- 每个阶段的 diff 以文件移动 / helper 提取为主，不夹带行为修复。
- 对应阶段测试、`git diff --check` 和 `graphify update .` 完成。

## 验收结果

- 最终构建通过：`cd cad-core && cmake --build build`。
- 最终测试通过：`python3 -m unittest tests.test_include_boundaries tests.test_feature_flows tests.test_p7_features tests.test_p8_features tests.test_adapters`，210 个测试 OK。
- 最终检查项：`git diff --check -- cad-core docs/框架` 通过；`graphify update .` 完成。
