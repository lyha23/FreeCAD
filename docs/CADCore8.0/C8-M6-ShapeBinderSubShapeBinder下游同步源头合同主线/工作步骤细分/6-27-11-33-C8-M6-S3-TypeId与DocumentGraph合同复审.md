# C8-M6 S3 TypeId 与 DocumentGraph 合同复审

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

## 非目标

- 不把完整 FreeCAD Document session 或 GUI 编辑状态同步给下游。
- 不把 `ReferenceShadow.brep` 例外扩展成完整 BREP 传输。
