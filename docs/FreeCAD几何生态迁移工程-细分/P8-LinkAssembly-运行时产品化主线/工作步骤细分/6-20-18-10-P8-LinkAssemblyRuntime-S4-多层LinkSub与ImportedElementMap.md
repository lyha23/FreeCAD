# P8-LinkAssemblyRuntime S4 多层 LinkSub 与 Imported ElementMap

## 目标

补齐复杂多层 LinkSub、LinkGroup、plain group、source alias、target prefix、imported shape ElementMap 和 Link retag history。S4 的重点是统一 subname / ElementMap 生命周期，不允许在 output 层按字符串修剪。

## 必读

- S0-S3 更新后的矩阵和实现结论。
- `src/App/Link.cpp`
- `src/App/PropertyLinks.cpp`
- `src/App/DocumentObjectGroup.cpp`
- `cad-core/src/app/link.cpp`
- `cad-core/src/part/property_topo_shape.cpp`
- `cad-core/src/topo/`
- `cad-core/fixtures/p8/app-link-multilevel-*.json`
- `cad-core/fixtures/p8/app-link-linked-plain-group-*.json`
- `cad-core/fixtures/p8/app-link-stable-history-*.json`
- `cad-core/fixtures/p8/app-link-full-sublist-external-tag.json`

## 实现要求

- 多层 LinkSub 不建立专用旁路；必须消费统一 Link ledger、PropertyLinkSub / XLink、mapped postfix 和 ElementMap / retag history。
- imported shape 的 subshape map / ElementMap 必须能被 Link chain 消费，不能只服务直接 Import display。
- 若 OCCT import topology 或 expected 基线不一致，先归类环境 / oracle / backendGap，不直接改 expected 或放宽断言。

## 非目标

- 不补完整 Part import/export 产品线。
- 不把 BREP 作为跨请求前端或后端状态。
- 不靠 bbox、shape order 或字符串猜测 source edge / face ownership。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_p6_topology tests.test_expected_fixtures
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core
```
