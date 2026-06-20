# 【已实现】P8-LinkAssemblyRuntime S4 多层 LinkSub 与 Imported ElementMap

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
- `cad-core/fixtures/p8/app-link-imported-element-map-chain.json`

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

## 完成结论

S4 已完成。当前 `cad-core/src/app/link.cpp` 与 `cad-core/src/part/topo_shape*.cpp` 已按 P8LAR-SRC-007/008/009 统一消费 Link ledger、PropertyLinkSub / XLink、mapped postfix、source alias、target prefix、plain group `_ChildCache` 等价遍历和 ElementMap / retag history；本轮未新增 output 层字符串修剪，也未按 fixture 名或 bbox/order 做特判。

本轮新增 `cad-core/fixtures/p8/app-link-imported-element-map-chain.json` 和 `test_p8_app_link_consumes_imported_element_maps_through_link_chain`，把 BREP、STEP、IGES imported shape 的 stable ElementMap 分别经中间 App::Link、LinkSub 和下游 LinkGroup child map 消费；既证明 imported ElementMap 不只服务直接 Import display，也覆盖 LinkGroup 聚合后的 child map 传播。已有 multilevel label-qualified LinkSub、linked plain group / nested plain group、ElementList nested plain group、FullSubList external mapped alias、stable split/deleted history 和 merge history retag tests 继续作为 S4 回归基线。

验证结果：`cmake --build build` 通过；`python3 -m unittest tests.test_p8_features tests.test_p6_topology tests.test_expected_fixtures` 通过 184 个测试，跳过 14 个 solver / oracle 条件用例；`git diff --check -- docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线 cad-core` 通过；队列工具确认 S4 已跳过，剩余 pending 为 S5 / S6。S5 仍作为 AssemblySolver 扩展步骤排队，但当前没有从旧 JointType / MarkerPlacement / DistanceType 队列继承的可直接实现 blocker；S6 仍保留最终 Worker / WASM / Web runtime 合同冻结。
