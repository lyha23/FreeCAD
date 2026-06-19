# 【已实现】LOFT-S2 capability 发布

已更新 CADCore3.0 文档、oracle 队列、C API capability metadata 和 adapter tests。只发布 expected-backed `Part::Loft` 第一批范围：source-backed `DocumentObject`、`Sections=App::PropertyLinkList`、`Solid` / `Ruled` / `Closed` / `MaxDegree`、`loft_thru_sections` maker history，以及五个 c3m4 fixtures。

未发布范围保持 remaining boundary：`Linearize=true` 后处理、face / vertex profile expected、复杂 profile family、Sweep / Filling / GeomPlate / PipeShell 和完整 Part surface family。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters
```
