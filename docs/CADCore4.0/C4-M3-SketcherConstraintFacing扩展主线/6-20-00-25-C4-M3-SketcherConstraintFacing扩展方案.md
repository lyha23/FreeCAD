# C4-M3 Sketcher Constraint-facing 扩展方案

## 目标

扩展 CAD Core 面向 Sketcher solver 的输入、约束状态、diagnostics 和几何更新入口。此专题包明确排除完整 GCS solver，实现只覆盖 cad-core recompute、引用恢复和前端运行时需要的语义。

## 范围

- FreeCAD 源码依据：`src/Mod/Sketcher/App/SketchObjectConstraints.cpp`、`SketchObjectGeometry.cpp`、`SketchObject.cpp`。
- cad-core 落点：`cad-core/src/sketcher`、`cad-core/src/runtime`、`cad-core/src/adapters/c_api/c_api.cpp`。
- 验收：`tests.test_p5_sketch`、`tests.test_adapters`。

## 阶段

| 步骤 | 内容 |
| --- | --- |
| S0 | constraint-facing DTO / status source audit |
| S1 | diagnostics 和 geometry update fixture |
| S2 | capability / adapter schema 或 deferred 收口 |

## 非目标

- 不迁移完整约束求解器。
- 不把 solver iteration、rank analysis 和 UI 解算反馈当作 4.0 必做。
- 不用 adapter 端 patch 弥补 sketcher core 语义。
