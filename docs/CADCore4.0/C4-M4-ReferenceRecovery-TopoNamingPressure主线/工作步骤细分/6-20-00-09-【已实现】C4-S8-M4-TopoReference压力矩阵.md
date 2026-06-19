# 【已实现】C4-S8 M4 Topo / Reference 压力矩阵

## 目标

先设计长期编辑压力矩阵，不直接改代码。矩阵必须覆盖多 producer、rename、Link retag、CopyOnChange、import change、ShapeFix / Refine / Boolean / DressUp / transformed 混合链。

本步已新增包内矩阵 `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/矩阵/topo_reference_pressure_matrix.tsv`，并同步全局 source / oracle / scope / blocker / validation / non-goal 行。矩阵包含 12 个 S9 planned fixture rows，分类覆盖 `updated`、`unchanged`、`needs_reselect`、`diagnostic_only`。

## 必读文件

- `docs/CADCore4.0/C4-M4-ReferenceRecovery-TopoNamingPressure主线/6-19-23-57-C4-M4引用恢复与TopoNaming压力回归方案.md`
- `docs/CADCore4.0/矩阵/cadcore4_scope_review_matrix.tsv`
- `src/App/PropertyLinks.cpp`
- `src/App/ElementMap.cpp`
- `src/App/MappedName.cpp`
- `src/Mod/Part/App/TopoShape.cpp`
- `src/Mod/Part/App/TopoShapeExpansion.cpp`
- `cad-core/src/app`
- `cad-core/src/part/topo_shape.cpp`

## 产物

- 新增或更新 pressure fixture rows。
- 分类每个场景为 `updated`、`unchanged`、`needs_reselect`、`diagnostic_only`。
- 写清 source ownership、terminal history、elementReferenceUpdates / documentObjectUpdates 预期。

## S9 输入边界

- S9 只实现 `topo_reference_pressure_matrix.tsv` 中 `C4M4-TR-PRESS-001..012` 的 fixture / expected / diagnostics / capability 发布。
- 若 S9 发现必须新增场景，先更新矩阵并同步全局行，不在实现中临场新增未登记 fixture 字段。
- ReferenceShadow 只允许单 subshape snapshot；split / deleted / ambiguous terminal history 必须保持 `needs_reselect` 或 `diagnostic_only`，除非 source ownership 唯一解析到一个 current terminal element。

## 非目标

- 不用输出顺序或 bbox 作为稳定引用依据。
- 不把 split / ambiguous relation 写成唯一 alias。
- 不实现代码。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0
```

## 完成口径

C4-M4 pressure matrix 能指导后续实现，不需要 worker 临场发明场景和验收字段。
