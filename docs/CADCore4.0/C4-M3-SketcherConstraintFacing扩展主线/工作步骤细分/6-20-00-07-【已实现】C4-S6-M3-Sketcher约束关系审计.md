# 【已实现】C4-S6 M3 Sketcher solver-facing 约束关系审计

## 目标

审计前端会发出的 Sketcher constraint DTO，确定哪些属于 4.0 solver-facing 扩展。只补输入、状态、diagnostics、request-local geometry update；不实现完整 GCS solver。

## 必读文件

- `docs/CADCore4.0/C4-M3-SketcherExternalGeometry总览/6-19-23-56-C4-M3SketcherExternalGeometry扩展方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_non_goal_registry.tsv`
- `src/Mod/Sketcher/App/Sketch.cpp`
- `src/Mod/Sketcher/App/SketchObject.cpp`
- `src/Mod/Sketcher/App/SketchObjectConstraints.cpp`
- `src/Mod/Sketcher/App/PropertyConstraintList.cpp`
- `cad-core/src/sketcher/sketch_object.cpp`
- `cad-core/src/sketcher/sketch_object_constraints.cpp`
- `cad-core/tests/test_p5_sketch.py`

## 产物

- constraint DTO 矩阵：supported / deferred / diagnostic / non-goal。
- Fixture 草案：新增 relation 的 solver_state、geometry_updates、profile_ready、diagnostic codes。
- 更新 `cadcore4_fixture_oracle_matrix.tsv` 和 blocker queue。

## 非目标

- 不保存 solver session。
- 不引入完整 GCS 求解器。
- 不用 fake profile 掩盖 unsupported relation。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p5_sketch tests.test_adapters
```

## 完成口径

Sketcher 4.0 constraint expansion 有清晰 DTO/source/diagnostic 队列；unsupported relation 有稳定 failure surface。
