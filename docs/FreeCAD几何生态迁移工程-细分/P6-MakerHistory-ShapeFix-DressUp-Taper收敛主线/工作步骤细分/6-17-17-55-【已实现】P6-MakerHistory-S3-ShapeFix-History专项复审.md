# P6 MakerHistory S3 ShapeFix History 专项复审

## 目标

复核 ShapeFix / ShapeBuild_ReShape producer 是否已被 cad-core topo history 正式消费。S3 不假设 `covered_no_generated_producer` 一定正确；必须用 FreeCAD source、cad-core source、capability 和 focused tests 验证。

## 执行结论

2026-06-17 已完成 S3 复审：`P6MH-SCOPE-002` 裁决为 `supported`。FreeCAD 的 `MapperHistory(ShapeFix_Root&)` 与 `ShapeBuild_ReShape` 入口在 cad-core 中分别落到 `ShapeFixHistory`、`namedShapeForShapeFixHistory()` 和 `namedShapeForShapeFixRootHistory()`；`test_p6_topology.py` 约束 small-edge deleted history 与 ShapeFix_Root modified history；`test_adapters.py` 约束 `producer_matrix.shape_fix.status == covered_no_generated_producer` 且 `remaining == []`。当前没有 ShapeFix backendGap。

`generated_empty_review` 不是输出端修剪，也不是需要合成 generated 的缺口；当前证据表明它是已审计的非 producer 路径。复杂 split / deleted 引用恢复仍保留在 `P6MH-SCOPE-005 = notCollected`，只有后续 oracle 证明 mismatch 时才进入 S6 C++。

## FreeCAD 依据

| 入口 | 关键点 |
| --- | --- |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShape.h::MapperHistory(ShapeFix_Root&)` | ShapeFix_Root 是 MapperHistory 的正式输入 |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::ShapeFixModule::removeSmallEdges()` | `ShapeFix::RemoveSmallEdges(sh, tol, reshape)` 产生 ReShape evidence |
| `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/ShapeFix/*` | Python ShapeFix wrappers 是能力来源，但本主线只消费几何 history |

## cad-core 复核点

| 文件 | 检查项 |
| --- | --- |
| `cad-core/src/part/shape_fix.cpp` | `ShapeFixHistory::perform()`、`removeSmallEdges()`、`Modified()`、`Generated()`、`IsDeleted()` 是否接入 context history |
| `cad-core/src/part/topo_shape.cpp` | 是否把 ShapeFix history 转成 NamedShape / ElementMap / terminal diagnostics |
| `cad-core/src/adapters/c_api/c_api.cpp` | `producer_matrix.shape_fix` 是否准确描述 covered / remaining |
| `cad-core/tests/test_adapters.py` | 是否约束 `shapefix_history`、`shapefix_generated_history` remaining gap |

## 范围裁决

| scope | S3 需要裁决 |
| --- | --- |
| `P6MH-SCOPE-002` | ShapeFix 是 `supported`、`releaseGate`、`notCollected` 还是 `backendGap` |
| `P6MH-SCOPE-005` | ShapeFix 后复杂 split / deleted 恢复是否已有 oracle |

## 必须回写的矩阵行

- `P6MH-SCOPE-002`
- `P6MH-BLOCK-002`
- `P6MH-BG-002`

## 验收标准

- S3 文档必须记录 ShapeFix generated 空路由是 FreeCAD 无 producer 证据，还是 cad-core 缺口。
- 如果仍是 `covered_no_generated_producer`，必须说明为什么不是 backendGap，并保留 capability/test 证据。
- 如果转 `backendGap`，必须给出 FreeCAD authority、cad-core mismatch、C++ landing 和 focused test。
- 执行：

```bash
rg -n "ShapeFixHistory|removeSmallEdges|Generated\\(|IsDeleted|shapefix" cad-core/src/part cad-core/include/cad_core/part cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py
rg -n "MapperHistory\\(ShapeFix|RemoveSmallEdges|FixVertexPosition" src/Mod/Part/App
git diff --check
```

## 非目标

- 不迁移完整 ShapeFix Python API。
- 不引入 ShapeFix 结果跨请求缓存。
- 不用输出端 pruning 伪装 generated history。
