# 【已实现】C8-M6 S3 TypeId 与 DocumentGraph 合同复审

## 目标

复核下游同步所需的 TypeId、DocumentObject graph、property-link 和 request / response DTO 合同，确保它们是 FreeCAD 仓库内的源头事实，而不是下游实现假设。

## 范围

纳入：

- `PartDesign::ShapeBinder`
- `PartDesign::SubShapeBinder`
- `PartDesign::SubShapeBinderPython`
- `Support` / `References` / `BindMode` / `TraceSupport` / `MakeFace` / `Offset2D` / `Fuse` / `Refine`
- `documentObjectUpdates` 和 `elementReferenceUpdates` 的 request-local 语义。

排除：

- 下游 registry 具体实现。
- 前端持久 geometry cache。
- FreeCAD GUI property editor lifecycle。

## 必须回写的矩阵行

- `C8M6-SYNC-101`
- `C8M6-SYNC-104`
- `C8M6-SCOPE-101`
- `C8M6-SCOPE-201` 到 `C8M6-SCOPE-204`
- `C8M6-BLOCKER-301`

## 复核命令

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "PartDesign::ShapeBinder|PartDesign::SubShapeBinder|SubShapeBinderPython|documentObjectUpdates|elementReferenceUpdates|BindMode|TraceSupport|MakeFace|Offset2D|Refine" cad-core/src cad-core/tests cad-core/fixtures/c8m1
cd cad-core
./cad-core capabilities > /tmp/c8m6-s3-capabilities.json
```

## 验收

- TypeIds 和 `/cad/capabilities` 输出能从 `capability_contract.cpp` 追溯。
- DTO 字段都有 fixture 或 focused test 证据。
- C8-M5 Body replay 裁决写入合同：无 input `Body.BaseFeature` 时不要求下游合成 `BodyBaseFeature`。

## S3 复核结论

- TypeId 源头成立：FreeCAD `src/Mod/PartDesign/App/ShapeBinder.h/.cpp` 声明 `PartDesign::ShapeBinder`、`PartDesign::SubShapeBinder`、`PartDesign::SubShapeBinderPython`，当前 `cad-core/src/runtime/capability_contract.cpp` 和 `/tmp/c8m6-s3-capabilities.json` 发布同一组 TypeIds。
- capability 合同成立：`part_design.shape_binder.status=supported_c8m1_expected_backed_request_local` 且 `remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap`，唯一 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- DocumentGraph 合同成立：`capability_contract.cpp` 发布 `source=DocumentObject graph`，对象字段为 `Name/ID/TypeId/Properties`，property-link 字段覆盖 `value`、`values`、`SubSet`、`SubList`、`StableSubList`、`FullSubList`、`ShadowSub`、`ReferenceShadow`、`ExternalFlags`、`Document`，`tests/test_adapters.py` 有 focused assertion。
- request / response DTO 合同成立：`runtime::recomputeResultJson()` 和 CLI legacy payload 均以 request-local 方式发布 `results`、`elementReferenceUpdates`、`documentObjectUpdates`、`diagnostics`、`binaryPayloads`；adapter capability assertion 锁定 `stateless_result_channels`。
- Binder property 合同成立：FreeCAD source 与 C8-M1 fixtures 覆盖 `Support` / `References` 等 link-sublist 输入，以及 `BindMode`、`TraceSupport`、`MakeFace`、`Offset2D` request fields、`Fuse`、`Refine`；`tests/test_c8_shapebinder.py` 锁定 capability、fixture expected、BindMode Detached `documentObjectUpdates` 和 CopyOnChange known-gap diagnostic。
- C8-M5 Body replay 裁决已写入合同：`cad-core/src/part_design/body.cpp` 只有 input graph 显式包含 `Body.BaseFeature` 时才发布 `body_basefeature_featurebase_create` 等 writeback；`shape-binder-subshape-binder-element-map-namedshape-body-replay` focused test 断言无 input `Body.BaseFeature` 时 `BodyBaseFeature` 不存在且 `documentObjectUpdates=[]`。
- BREP 边界未扩大：`ReferenceShadow.brep` 仍只作为单个旧 subshape snapshot 的 request-local 恢复证据；S3 不把它升级为完整 BREP、TopoDS_Shape、NamedShape、ElementMap 或 temporary document cache 跨请求传输。
- S3 未发现 current code、fixture、capability 或 docs 矛盾；没有新增 `unexpected_mismatch`，`C8M6-BLOCKER-301` 已关闭并转入 S4。

## S3 验收结果

- S3 `rg` 复核命令已运行，命中 TypeIds、`documentObjectUpdates`、`elementReferenceUpdates`、`BindMode`、`TraceSupport`、`MakeFace`、`Offset2D`、`Fuse`、`Refine` 在 `cad-core/src`、`cad-core/tests` 和 `cad-core/fixtures/c8m1` 的证据。
- `cd cad-core && ./cad-core capabilities > /tmp/c8m6-s3-capabilities.json` 已运行，capability payload 与 `capability_contract.cpp` 一致。
- C8-M6 TSV 字段数检查通过。
- C8-M6 包尾随空白扫描无匹配；`rg` exit 1 按仓库规则视为通过。
- `git diff --check` 通过。
- `step_goal_queue.py` 刷新后队列下一项为 S4。

## 非目标

- 不把完整 FreeCAD Document session 或 GUI 编辑状态同步给下游。
- 不把 `ReferenceShadow.brep` 例外扩展成完整 BREP 传输。
