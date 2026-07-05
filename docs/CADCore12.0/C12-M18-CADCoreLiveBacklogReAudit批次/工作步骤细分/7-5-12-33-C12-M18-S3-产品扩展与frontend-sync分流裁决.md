# C12-M18 S3 产品扩展与 frontend sync 分流裁决

把 product extension、backend gap 和前端消费缺口分开，避免把非后端问题写成 C++ 实现任务。

## 必读

- `../README.md`
- `../矩阵/c12m18_live_backlog_product_extension_split.tsv`
- `../矩阵/c12m18_live_backlog_non_goal_registry.tsv`
- `../../../capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md`
- `../../C12-M11-SketchInternalEdgeSubshapeMeshContract批次/README.md`
- `../../C12-M15-SketchStableGeometryIdMappedNameLedger设计批次/README.md`
- `../../C12-M16-SketchSplitFragmentGeometryIdentityLedger实现批次/README.md`

## 操作

1. 明确 PartDesign 几何共线 BSpline / 非 Line axis 继续作为 product extension 保留。
2. 复核 C12-M11 open wire raw EdgeN、C12-M15 stable geometry id、C12-M16 split fragment ledger 的后端当前状态。
3. 若剩余工作是 my-chili3d consumer sync，只记录为 frontend package candidate，不创建 FreeCAD/cad-core C++ work。
4. 检查 `docs/capability` wording 是否还把已整改项列为 current non-native parity。
5. 验证后把本文件重命名为带 `【已实现】` 的同名文件。

## 非目标

- 不改 my-chili3d。
- 不删除已批准 product extension。
- 不把 frontend token consumer 缺口写成 backend geometry gap。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n "PartDesign 轴引用|SubtractivePipe product PipeLaw|product extension|frontend|my-chili3d" docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次 docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md
git diff --check
```

