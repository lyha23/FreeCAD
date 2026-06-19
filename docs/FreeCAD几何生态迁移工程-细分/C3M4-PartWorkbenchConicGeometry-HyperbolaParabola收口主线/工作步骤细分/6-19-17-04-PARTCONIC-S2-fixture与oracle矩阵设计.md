# PARTCONIC-S2 fixture 与 oracle 矩阵设计

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
