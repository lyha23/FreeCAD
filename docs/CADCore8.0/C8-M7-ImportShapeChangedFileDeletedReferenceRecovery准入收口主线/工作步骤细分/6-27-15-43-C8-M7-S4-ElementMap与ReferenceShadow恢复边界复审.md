# C8-M7 S4 ElementMap 与 ReferenceShadow 恢复边界复审

## 目标

复核 import ElementMap、owner-qualified alias、`ReferenceShadow` 和 `elementReferenceUpdates` 的边界，确保恢复逻辑只使用当前请求内证据。

## 必须复核

- `namedShapeForImportedShape()` 是否为 import shape 生成 `owner.ElementN` alias 和 `import_shape_element_map` mapper history。
- `resolveElementReference()` / `currentSubshapeForReference()` 是否优先走 current `NamedShape` ElementMap。
- `recoverSubshapeForReference()` 是否只在 current shape 中搜索，并用 request-local `ReferenceShadow` fingerprint / BREP 验证。
- `referenceShadowUpdateJson()` 是否刷新单 subshape snapshot，而不是发布完整对象 BREP。
- deleted-file 场景下没有 current shape 时，是否必须 diagnostic，不得用旧请求状态恢复。

## 必须回写

- `C8M7-SCOPE-201`
- `C8M7-SCOPE-202`
- `C8M7-SCOPE-203`
- `C8M7-SCOPE-204`
- `C8M7-BLOCKER-401`
- README 的 S4 结论。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'namedShapeForImportedShape|ImportElementMapSource|import_shape_element_map|resolveElementReference|recoverSubshapeForReference|referenceShadowUpdateJson|ReferenceShadow.brep' cad-core/src cad-core/include cad-core/tests
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
git diff --check
```

## 非目标

- 不扩大 `ReferenceShadow.brep` 例外。
- 不在 reference resolver 中按 fixture 名或几何排序猜测。
- 不新增持久 ElementMap / NamedShape 存储。
