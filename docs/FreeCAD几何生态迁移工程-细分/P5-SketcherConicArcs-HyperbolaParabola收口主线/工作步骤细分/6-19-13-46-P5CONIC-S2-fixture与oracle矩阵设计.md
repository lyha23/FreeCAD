# P5CONIC-S2 fixture 与 oracle 矩阵设计

## 目标

为双曲线弧和抛物线弧建立可验证 fixture 组合，先定义验收口径，再进入实现补齐。避免继续保留过期 unsupported case。

## 必读文件

- `/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/mvp/unsupported-geometry.json`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/p5/sketch-unsupported-hyperbola.json`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_diagnostics.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_mvp.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P5-SketcherConicArcs-HyperbolaParabola收口主线/矩阵/p5_conic_arcs_scope_review_matrix.tsv`

## 操作

1. 设计最小 fixture 批次：
   - `sketch-hyperbola-arc-profile`：有效 profile edge。
   - `sketch-parabola-arc-profile`：有效 profile edge。
   - `sketch-conic-arcs-construction-filter`：construction 几何不进入 profile。
   - `sketch-conic-arcs-external-geometry`：外部几何合并后计数稳定。
   - `sketch-invalid-conic-arc-params`：非法半径、focal 或 trim 参数输出 diagnostics。
2. 判断每个 fixture 是否需要 FreeCAD expected：
   - profile/external 几何数量和 bbox 若能用现有 cad-core runner 稳定约束，可先写 focused unit。
   - 若涉及 FreeCAD shape/subshape oracle，则使用 `collect_freecad_expected.py` 采集，并记录 FreeCADCmd/OCCT 基线。
3. 把旧 `sketch-unsupported-hyperbola` 重新归类：
   - 如果 ArcOfHyperbola 已支持，改成支持 fixture 或删除 unsupported 期望。
   - 如果要保留 unsupported fixture，必须换成真实未支持几何类型，不得继续用 hyperbola。
4. 更新矩阵和 blocker。

## 非目标

- 不为单个 fixture 名称写分支。
- 不放宽 diagnostics 断言掩盖能力口径变更。
- 不把 native FreeCAD 采集失败等同于 cad-core 实现失败。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_diagnostics
python3 -m unittest tests.test_mvp
```

## 完成条件

fixture/test 计划能同时覆盖 hyperbola、parabola、construction、external、invalid params，且旧 unsupported hyperbola 口径有明确处理决定。
