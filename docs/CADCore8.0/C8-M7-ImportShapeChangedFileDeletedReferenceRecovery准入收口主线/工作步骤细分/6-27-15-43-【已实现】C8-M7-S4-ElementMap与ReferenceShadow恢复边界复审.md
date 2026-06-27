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

## 复审结论

S4 复核当前源码和 tests 后不打开代码闸门：

- `namedShapeForImportedShape()` 已从当前导入 shape 建立 `NamedShape`，为每个当前元素写入 `owner.ElementN` alias、element sources，并记录 `import_shape_element_map` preserved mapper history；P6 import STEP / BREP tests 覆盖 Face / Edge alias 与 mapper history。
- `currentSubshapeForReference()` 先通过 current `NamedShape` 的 `ElementMap` 调 `part::resolveElementReference()`；只有 current `NamedShape` 不能解析时才回退到 current shape 的 subshape name 查找。
- `recoverSubshapeForReference()` 要求 `view.shapes` 中存在当前请求对象，只在当前 `shape` 或当前 `InternalShape` 里用 request-local `ReferenceShadow` fingerprint / BREP 做唯一性恢复；没有 current shape 时返回 missing 并进入 diagnostic。
- `referenceShadowUpdateJson()` 只接收一个 resolved `currentSubshape`，刷新 fingerprint，并且只在输入已有 `ReferenceShadow.brep` 时刷新该单 subshape snapshot；不发布完整对象 BREP。
- deleted-file 且没有 current imported shape 时必须保持 diagnostic / `diagnostic_non_goal`，不得用旧请求 `NamedShape`、`ElementMap`、`TopoDS_Shape` 或完整 BREP cache 恢复。

回写结果：

- `C8M7-SCOPE-201`、`C8M7-BG-201`：`already_covered`。
- `C8M7-SCOPE-202`、`C8M7-BG-103`：由 `request_local_backend_gap_candidate` 收口为 `already_covered`。
- `C8M7-SCOPE-203`、`C8M7-BG-203`：`already_covered`，并锁定 `ReferenceShadow.brep` 单 subshape snapshot 边界。
- `C8M7-SCOPE-204`：保持 `diagnostic_non_goal`。
- `C8M7-BLOCKER-401`：`Closed S4`，S5 继续处理 capability residual 发布口径，S6 不因 S4 打开实现。

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
