# C3M4 Part Workbench Surface Family 冻结收口方案

## 目标

在 RuledProjection、Loft、Sweep/PipeShell、Filling、GeomPlate 全部完成后，冻结 cad-core 后端必迁 Part surface family 能力：统一 capability、oracle fixture 队列、adapter metadata、docs 和最终验证。

## 冻结范围

- `Part::RuledSurface`
- `Part::ProjectOnSurface` 已验证窄批或明确独立后续边界
- `Part::Loft`
- `Part::Sweep`
- low-level PipeShell backend
- `Part.makeFilledFace()` / Filling backend
- `Part.GeomPlate` geometry backend

## 发布规则

- 只发布 fixtures 和 expected 保护过的范围。
- package-local non-goal 必须回写为全局 remaining boundary 或新队列，不能留在旧文档里误导。
- `docs/CADCore3.0/capabilities-gap对照表.md` 中 `part_workbench.conic_curves` 的 surface remaining gaps 要迁移到新的 surface family capability。
- C API capability metadata 必须列出 payload keys、covered fixtures、diagnostic codes、remaining gaps。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分 docs/CADCore3.0 cad-core

cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_mvp tests.test_diagnostics tests.test_p5_sketch tests.test_p6_topology tests.test_p7_features tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```
