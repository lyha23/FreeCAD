# Material 资源解析与无状态图边界方案

## 结论

Material card、library、model 文件、用户参数、manager cache 和 refresh 属于 FreeCAD 主机资源管理，不进入 CAD Core 的无状态 `DocumentObject graph`。请求图继续只携带已经解析完成的 `Materials::PropertyMaterial` property map；本批没有新增 library URI、card path、model UUID 或继承链 DTO。

现有三条 `material-properties` native fixture 继续验收 resolved map、第二次 recompute 和 `Part::Feature.ShapeMaterial` 公开投影。主机资源分支改由 hermetic `native_process_test` 收口，不能据此宣称 CAD Core runtime parity。

## FreeCAD 依据和调用链

```text
/Users/li/Chili3DProject/FreeCAD/src/Mod/Material/App/AppMaterial.cpp::initModule
  -> MaterialManager.cpp::Materials::MaterialManager
  -> MaterialManagerLocal.cpp::{getConfiguredLibraries,refresh}
  -> ModelLoader.cpp::{getModelLibraries,loadLibrary}
  -> MaterialLoader.cpp::{loadLibrary,dereference}
  -> Materials.cpp::Materials::Material
  -> PropertyMaterial.cpp::Materials::PropertyMaterial::setPyObject
  -> PartFeature.cpp::Part::Feature::getMaterialAppearance
```

`getConfiguredLibraries()` 与 `getModelLibraries()` 读取 `UseBuiltInMaterials`、`UseMaterialsFromWorkbenches`、`UseMaterialsFromConfigDir`、`UseMaterialsFromCustomDir` 和 `CustomMaterialsDir`，并扫描宿主文件系统。`PropertyMaterial::setPyObject()` 接收的是已解析的 `Materials.Material`；`Part::Feature` 再把该值投影为 ShapeMaterial/appearance。这是资源解析与无状态建模图的稳定切面。

## Hermetic process contract

机器可读契约为 `process-contract/material-resolution`，报告写入：

```text
cad-core/tools/freecad_expected_parity/reports/process_contract/material-resolution.v1.json
```

采集器每个 case 都使用独立临时目录和独立 FreeCADCmd 进程，关闭 built-in、workbench 和 user-config material sources，只开放签入的 `CustomMaterialsDir`；同时记录 argv、`FREECAD_USER_HOME`/`DATA`/`TEMP`、exit/signal/timeout、stdout/stderr、producer path/SHA、tool/resource SHA。两轮必须语义一致。

8 个分支为：本地 card/model UUID 与 path 定位、父子继承和 override、manager refresh 后第二次解析、missing card、unknown model、invalid model schema、invalid property type、inheritance cycle。当前受控 producer 的 cycle 分支稳定以 signal 11 终止；这是 `MaterialLoader::dereference()` 在递归完成后才标记 dereferenced、缺少 in-progress cycle guard 的 source-backed 原生边界，不生成 CAD graph 结果。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 cad-core/tools/collect_material_resolution_contract.py --repeat 2
python3 -m unittest cad-core.tests.test_material_resolution_contract \
  cad-core.tests.test_audit_freecad_fixture_authority
python3 cad-core/tools/validate_freecad_expected_ledger.py --all --strict \
  --report cad-core/tools/freecad_expected_parity/reports/ledger-strict-validation.v1.json
```

process receipt 缺失、schema/id 不符、repeat 小于 2、producer/tool 身份缺失、case/run 失败或 process 字段不完整时，API/capability audit fail closed。
