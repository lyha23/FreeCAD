# C5-M20 Filling precise blocker probe 记录

状态：`done_c5m20_precise_blocker_probe`

## 基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `FreeCADCmd`：`/home/user/.local/bin/FreeCADCmd`
- FreeCAD 版本：`1.2.0 revision 20260519`
- probe 脚本：`docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/docs/temp/6-23-19-17-c5m20-filling-precise-blocker-probe.py`
- 命令模板：

```bash
cd /home/user/Chili3DProject/FreeCAD
C5M20_PROBE_CASE=<case> timeout 60s /home/user/.local/bin/FreeCADCmd -c "exec(compile(open('docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M20-PartWorkbenchFillingPreciseBlockerRecovery主线/docs/temp/6-23-19-17-c5m20-filling-precise-blocker-probe.py', encoding='utf-8').read(), 'c5m20_filling_probe.py', 'exec'))"
```

## FreeCAD 依据

- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/AppPartPy.cpp::makeFilledFace()` 暴露 `surface`、`supports`、`orders`、constructor kwargs。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementFilledFace()` 构造 `BRepOffsetAPI_MakeFilling`，调用 `LoadInitSurface` 和 `maker.Add(...)`。
- `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp` 暴露 mutable direct wrapper；本包只把它作为 native owner 对照，不把 wrapper lifecycle 发布为 cad-core API。

## Probe 矩阵

| case | 输入形态 | 结果 | 分类 | expected-backed | delete condition |
| --- | --- | --- | --- | --- | --- |
| `default_control` | closed boundary wire | Face，edges=4，bbox `[-4.8,12.8]` | control | 已有 c3m4/c5m13 控制 | 不作为 C5-M20 新能力 |
| `surface_only` | `Part.makeFilledFace([wire], surface=face)` | SIGSEGV；栈在 `Wrapped_ParseTupleAndKeywords` / `Part.so`；shell exit 1 | native helper parse crash | no | `surface=` 返回稳定 `shape_summary` 后删除 |
| `boundary_support_only` | boundary edge support face，无 order | `CADKernelError: Failed to created face by filling edges` | native runtime blocker | no | support-only helper 返回稳定 expected 或明确可发布诊断后删除 |
| `boundary_order_g1_only` | boundary edge order G1，无 support | timeout 60s，exit 124 | native timeout blocker | no | order-only helper 60s 内稳定返回 expected / diagnostic 后删除 |
| `boundary_support_order_g1` | boundary edge support + G1 | `CADKernelError: Failed to created face by filling edges` | native runtime blocker | no | support/order G1 返回稳定 expected 或明确可发布诊断后删除 |
| `boundary_support_order_g2` | boundary edge support + G2 | `OCCError: Standard_ConstructionError: GeomPlate : the degree resolution must be upper of 2` | native runtime blocker | no | support/order G2 返回稳定 expected 或明确可发布诊断后删除 |
| `pts_on_curve_only` | `ptsOnCurve=16` | SIGSEGV；栈含 `PyObject_IsTrue` / `Part.so` | native helper parse crash | no | `PtsOnCurve` 单字段返回稳定 expected 后删除 |
| `anisotropy_only` | `anisotropy=True` | timeout 60s，exit 124 | native timeout blocker | no | `Anisotropy` 单字段返回稳定 expected 后删除 |
| `tol_g1_g2_only` | `tolG1=0.02,tolG2=0.2` | SIGSEGV；栈含 `GeomPlate_MakeApprox` / `BRepFill_Filling::Build` | OCCT build crash | no | `TolG1+TolG2` 单字段返回稳定 expected 后删除 |
| `max_segments_only` | `maxSegments=10` | SIGSEGV；栈在 `Wrapped_ParseTupleAndKeywords` / `Part.so` | native helper parse crash | no | `MaxSegments` 单字段返回稳定 expected 后删除 |
| `all_params` | all explicit constructor kwargs | SIGSEGV；栈在 `Wrapped_ParseTupleAndKeywords` / `Part.so` | native helper parse crash | no | 所有 explicit params 子集都稳定后再删除 all-params blocker |
| `nonboundary_support_order_g1` | boundary wire + non-boundary edge support/order G1 | `OCCError: Standard_ConstructionError: GeomPlate : the degree resolution must be upper of 2` | native runtime blocker | no | non-boundary support/order G1 返回稳定 expected 后删除 |
| `nonboundary_support_order_g2` | boundary wire + non-boundary edge support/order G2 | timeout 60s，exit 124 | native timeout blocker | no | non-boundary support/order G2 返回稳定 expected 后删除 |
| `wrapper_surface_control` | direct `Part.BRepOffsetAPI.MakeFilling().loadInitSurface(face)` | direct wrapper builds Face，edges=4，bbox `[-0.4,8.4]` | low-level owner control | no, not helper expected | 只作为 source-audited evidence；不替代 `Part.makeFilledFace(surface=...)` |
| `wrapper_support_order_g1_control` | direct wrapper `add(edge, face, 1, True)` | direct wrapper builds Face，edges=4，bbox `[-4.8,12.8]` | low-level owner control | no, not helper expected | 只作为 source-audited evidence；不替代 `Part.makeFilledFace(supports/orders=...)` |

## 结论

- 本轮没有任何请求范围内的 `Part.makeFilledFace(...)` precise blocker 能稳定采集 geometry expected。
- Direct wrapper control 证明低层 `BRepOffsetAPI_MakeFilling` 可在部分形态 build，但 cad-core 的产品边界是 request-local `Part.makeFilledFace` helper DTO，不保存或暴露 mutable Python builder lifecycle。
- 因此不更新 collector supported path，不新增 fixtures/expected，不修改 `part_filling.cpp`，只更新 docs/matrices/capability evidence 口径。
