# P5P6-S3 ExternalGeometry 状态机专项复审

## 目标

P5P6-S3 单独裁决 `ExternalGeometryExtension` 状态机进入本主线的边界。它关闭 Defining / Frozen / Detached / Missing / Sync 与完整 solver、live editing、Python API 之间的高风险混淆。

## FreeCAD 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/SketchObjectExternal.cpp::SketchObject::rebuildExternalGeometry()`
- `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryExtension.*`
- `~/Chili3DProject/FreeCAD/src/Mod/Sketcher/App/ExternalGeometryFacade.*`
- `~/Chili3DProject/FreeCAD/src/App/GeoFeature.cpp::GeoFeature::resolveElement()`

关键源码语义：

- Missing ref 会尝试通过 `GeoFeature::resolveElement()` 恢复旧 subname。
- Frozen 且没有 Sync 时跳过刷新。
- Sync 允许本次刷新，随后清除 Sync。
- Defining 可影响外部几何是否参与 profile 语义。
- 失败路径设置 Missing 并保留 diagnostic。

## 复审范围

| 范围 | P5P6 状态草案 | 说明 |
| --- | --- | --- |
| ExternalGeometry explicit edge / vertex / face projection | supported 或复核 | 已有 P5/P6 子集，S3 只复核是否继续走统一 resolver。 |
| `Defining` external profile | `notCollected` | 需要 FreeCAD oracle 区分 defining 与 reference-only 外部几何。 |
| `Frozen` | `notCollected` | 需要 oracle 固定源对象变化后不刷新或诊断行为。 |
| `Frozen + Sync` | `notCollected` | 需要 oracle 固定本次刷新、清 Sync、保留 Frozen 的行为。 |
| `Detached` | `notCollected` | 需要 oracle 固定不再追随源对象的行为。 |
| `Missing` / deleted target | `backendGap` 或 `notCollected` | 必须通过统一 resolver 和 ReferenceShadow evidence，不在 sketcher 中猜。 |
| complete Sketcher solver / live editing | `nonGoal` | 不进入本主线。 |
| Python facade / session API | `nonGoal` | 不进入后端 request/response 协议。 |

## 必须回写的矩阵行

- `P5P6-SCOPE-007`：Defining state。
- `P5P6-SCOPE-008`：Frozen / Frozen + Sync。
- `P5P6-SCOPE-009`：Detached。
- `P5P6-SCOPE-010`：Missing / deleted external reference recovery。
- 对应 blocker：`P5P6-BLOCK-007` 到 `P5P6-BLOCK-010`。

## Oracle 要求

S3 进入实现前至少补：

- Defining external profile vs reference-only projection。
- Frozen source changed without Sync。
- Frozen + Sync source changed。
- Detached source changed。
- Missing object / missing subshape / deleted target。

FreeCAD expected 必须来自本地 FreeCAD 行为或 focused probe，不能从 cad-core 当前输出倒推。

## 验收

文档/矩阵阶段：

```bash
git diff --check
```

实现阶段：

```bash
cd cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
```

再按实际新增 fixture 跑 P5/P6 ExternalGeometry filter。

## 非目标

- 不实现完整 Sketcher solver。
- 不实现 GUI addExternal / copyExternal / editor workflow。
- 不保存跨请求 external geometry cache。
- 不把 Missing 恢复绕过统一 resolver。
