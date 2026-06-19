# 【已实现】PARTCONIC-S2 fixture 与 oracle 矩阵设计

## 目标

把 Part geometry Hyperbola / Parabola 的 oracle 和 fixture 批次设计清楚，再进入实现。重点是 finite edge、invalid params、Topo curve metadata 和一个 Part consumer。

## 必读

- S1 已实现后的方案与矩阵。
- `cad-core/tools/collect_freecad_expected.py`
- `cad-core/fixtures/p8/part-ellipse.json`
- `cad-core/fixtures/p8/expected/part-ellipse.freecad.json`
- `cad-core/tests/test_p8_features.py`
- `src/Mod/Part/App/HyperbolaPyImp.cpp`
- `src/Mod/Part/App/ArcPyImp.cpp`
- `src/Mod/Part/App/TopoShapeEdgePyImp.cpp`

## 工作内容

1. 新增或更新 fixture/oracle 矩阵，至少列出：
   - `part-hyperbola-edge`
   - `part-parabola-edge`
   - `part-conic-edge-invalid-params`
   - `part-conic-edge-extrusion` 或明确 blocked reason
2. 设计 collector FreeCAD 路径：优先用 `Part.Hyperbola` / `Part.Parabola` + `Part.ArcOf*` / `toShape()`，记录 FreeCAD 版本基线。
3. 明确每个 fixture 断言：diagnostics、shape label、edge count、bbox、length、subshape map、curve kind metadata、expected parity。
4. 更新 blocker queue，只有 oracle 路径设计清楚后才能进入 S3。

## S2 live 结论

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- HEAD：`8f5f2afe43`。
- 最新提交：`8f5f2afe43 docs: 完成PARTCONIC S1边界审计`。
- 工作区边界：本轮开始时仍只有既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`；S2 只修改本 C3M4 主线 docs/矩阵文件，不回退或暂存这些 Sketcher 改动。
- 新增矩阵：`矩阵/part_conic_geometry_fixture_oracle_matrix.tsv`，覆盖 `part-hyperbola-edge`、`part-parabola-edge`、`part-conic-edge-invalid-params`、`part-conic-edge-extrusion`。
- Oracle 路径：有效 edge fixture 使用 `Part.Hyperbola` / `Part.Parabola` 构造 base conic，`Part.ArcOfHyperbola` / `Part.ArcOfParabola` 以 `True` sense 做有限 trim，再 `toShape()` 采集 edge、bbox、length、subshape map 和 curve kind metadata。
- `PARTCONIC-BLOCK-003`：S2 已关闭“路径设计”部分；collector 代码、fixture JSON 和 expected JSON 仍未实现，状态推进到 `open-S3`，不得写成 expected 已通过。
- Consumer：`part-conic-edge-extrusion` 仍归 S4；blocked reason 是 `PartConicCurveDTO` 是请求级 payload，FreeCAD 没有 `Part::Hyperbola` / `Part::Parabola` DocumentObject Base，现有 collector 还不能把 transient conic edge producer 接到 `Part::Extrusion`。

## 非目标

- 不实现 cad-core。
- 不采集大批无关 Part primitive。
- 不用 BREP 请求替代 typed conic fixture。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchConicGeometry-HyperbolaParabola收口主线
```

完成后把本文件重命名为 `6-19-17-04-【已实现】PARTCONIC-S2-fixture与oracle矩阵设计.md`。
