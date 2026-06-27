# C8-M7 ImportShape changed-file / deleted-reference recovery 准入收口主线

## 定位

C8-M7 承接 C8-M6 之后的 live capability 残留：`topo_history.producer_matrix.import_shape.remaining=["changed_file_deleted_reference_recovery"]`。

本包不默认打开 C++ 实现。它先把这条残留拆成两类可裁决语义：

- changed-file：同一请求 graph 中 `FileName` 指向的 STEP / BREP / IGES 文件变化后，`cad-core` 是否已经能按当前文件重新导入并刷新 request-local `NamedShape` / ElementMap / mapper history。
- deleted-reference：文件已删除或不可读时，后端是否允许依赖跨请求缓存、完整 BREP、TopoDS_Shape、NamedShape 或 ElementMap 来恢复旧引用。

若 S0-S5 证明 changed-file recovery 只是 capability publication drift，S6 走 no-code 收口；若发现 request-local 可实现缺口，S6 才打开受限代码落点。deleted-reference 不得绕过 CAD Core 无状态边界。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- 建包起点：`HEAD=8a64f67808`（`8a64f67808 docs: 完成 C8-M6 S6 发布闸门`），开始工作区干净。
- C8-M1 到 C8-M6 工作步骤队列均已为空。
- 当前 `cad-core/src/runtime/capability_contract.cpp` 发布 `import_shape.status=done_first_slice`，`covered=["step","brep","iges","owner_qualified_alias"]`，唯一 `remaining=["changed_file_deleted_reference_recovery"]`。
- C7-M7 已裁决完整 imported ElementMap、ShowElement persistent writeback 和 cross-document hash / postfix lifecycle 没有 backend implementation gate，保持 `oracle_blocked` 或 `oracle_blocker`。
- C8-M7 不重开 C7-M7 的完整 Link / ShowElement / cross-document 持久生命周期；只处理当前 live capability 中仍暴露的 `import_shape` residual 是否应实现、保留或改发布。

## 与既有包的关系

- C7-M7：证明完整 imported ElementMap / LinkElement 持久写回 / cross-document lifecycle 缺 native oracle 或属于非无状态后端职责；C8-M7 不重开这些行。
- C8-M6：完成 ShapeBinder/SubShapeBinder 下游同步源头合同并清空队列；C8-M7 从 current capability 的下一条 active residual 继续。
- P6/P8 import tests：已有 STEP / BREP / IGES import first slice、owner-qualified alias 和 `import_shape_element_map` mapper history 证据；C8-M7 只复核 residual 是否仍应留在 `remaining`。

## 主文件

- 总入口：`6-27-15-37-C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线总入口.md`
- 方案：`6-27-15-37-C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 步骤队列

1. S0：live 基线与 residual 声明冻结。
2. S1：FreeCAD source 与 current coverage 复核。
3. S2：准入路由与 blocker 矩阵。
4. S3：import 文件生命周期 oracle 复审。
5. S4：ElementMap 与 ReferenceShadow 恢复边界复审。
6. S5：capability 残留与 non-goal 发布准入。
7. S6：实现或 no-code 发布闸门。

## 允许代码落点

只有 S3-S5 证明存在 request-local、source-backed、非跨请求缓存的实现缺口时，S6 才允许代码修改。候选落点限于：

- `cad-core/src/part/part_import.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/src/part/topo_shape.cpp`
- `cad-core/src/runtime/reference_resolution.cpp`
- `cad-core/src/runtime/element_reference_update.cpp`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p8_features.py`
- `cad-core/tests/test_adapters.py`

禁止用 adapter 字符串改写、fixture 名称分支、输出排序猜测、文件名特判、跨请求 shape cache、持久完整 BREP、持久 TopoDS_Shape、持久 NamedShape 或持久 ElementMap 关闭 residual。

## 验收分层

本轮短跑默认验收：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线 docs/CADCore8.0/README.md
git diff --check
```

阶段复核按 S3-S6 矩阵裁决后运行：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m7-capabilities.json
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_adapters
```
