# C8-M7 ImportShape changed-file / deleted-reference recovery 准入收口主线

## 定位

C8-M7 承接 C8-M6 之后的 live capability 残留：`topo_history.producer_matrix.import_shape.remaining=["changed_file_deleted_reference_recovery"]`。S6 已把该残留收口为 capability publication drift，当前 `import_shape.remaining=[]`。

本包不默认打开 C++ 实现。它先把这条残留拆成两类可裁决语义：

- changed-file：同一请求 graph 中 `FileName` 指向的 STEP / BREP / IGES 文件变化后，`cad-core` 是否已经能按当前文件重新导入并刷新 request-local `NamedShape` / ElementMap / mapper history。
- deleted-reference：文件已删除或不可读时，后端是否允许依赖跨请求缓存、完整 BREP、TopoDS_Shape、NamedShape 或 ElementMap 来恢复旧引用。

若 S0-S5 证明 changed-file recovery 只是 capability publication drift，S6 走 no-code 收口；若发现 request-local 可实现缺口，S6 才打开受限代码落点。deleted-reference 不得绕过 CAD Core 无状态边界。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- S0 live 基线：`HEAD=1450487296`（`1450487296 docs: 新增 C8-M7 ImportShape 恢复准入方案`），`git -c core.quotepath=false status --short -uall` 无输出。
- C8-M1 到 C8-M6 工作步骤队列均已为空；C8-M7 队列在 S0 重命名前首项为 S0。
- S0 时 `cad-core/src/runtime/capability_contract.cpp` 发布 `import_shape.status=done_first_slice`，`covered=["step","brep","iges","owner_qualified_alias"]`，唯一 `remaining=["changed_file_deleted_reference_recovery"]`；S6 后保持相同 status / covered，`remaining=[]`。
- C7-M7 已裁决完整 imported ElementMap、ShowElement persistent writeback 和 cross-document hash / postfix lifecycle 没有 backend implementation gate，保持 `oracle_blocked` 或 `oracle_blocker`。
- C8-M7 不重开 C7-M7 的完整 Link / ShowElement / cross-document 持久生命周期；只处理当前 live capability 中仍暴露的 `import_shape` residual 是否应实现、保留或改发布。

## 与既有包的关系

- C7-M7：证明完整 imported ElementMap / LinkElement 持久写回 / cross-document lifecycle 缺 native oracle 或属于非无状态后端职责；C8-M7 不重开这些行。
- C8-M6：完成 ShapeBinder/SubShapeBinder 下游同步源头合同并清空队列；C8-M7 从 current capability 的下一条 active residual 继续。
- P6/P8 import tests：已有 STEP / BREP / IGES import first slice、owner-qualified alias 和 `import_shape_element_map` mapper history 证据；C8-M7 只复核 residual 是否仍应留在 `remaining`。

## S1 源码与 current 覆盖结论

S1 已复核 FreeCAD 与 current `cad-core` 覆盖：FreeCAD `ImportBrep` / `ImportStep` / `ImportIges` 均以 `FileName` 为执行输入，文件不可读时走 `Cannot open file` 错误路径，读取成功后通过 `TopoShape::importBrep` / `importStep` / `importIges` 从当前文件重建 `Shape`，没有隐式旧后端 cache。

current `cad-core` 的 `Part::ImportBrep` / `ImportStep` / `ImportIges` 同样按请求中的当前 `FileName` 读取 BREP / STEP / IGES，并通过 `namedShapeForImportedShape()` 发布 current request 的 `NamedShape`、owner-qualified `ElementMap` alias 和 `import_shape_element_map` mapper history。`PropertyLinks` / `ElementMap` / `TopoShape` mapper 复核确认：引用恢复只能走当前 shape、stable subname / owner-qualified alias 和 request-local `ReferenceShadow` 证据；`ReferenceShadow.brep` 仍只是单 subshape snapshot evidence，不能扩展为完整对象 BREP 或跨请求缓存。

现有测试已经覆盖 `import_shape_element_map`、owner-qualified alias、BREP / STEP / IGES current-file import、imported ElementMap link-chain consumption 和 capability `import_shape` row。S1 不打开代码闸门；changed-file 与 deleted-file 的最终路由继续交给 S2-S5 裁决。

## S2 准入路由结论

S2 已把 `changed_file_deleted_reference_recovery` 拆成明确 owner：changed-file 当前可读文件重导入路由为 `already_covered`，S3 只保留文件生命周期复审；deleted / unreadable `FileName` 只允许显式 diagnostics，路由为 `known_gap_retained`，不得声明 deleted-file full recovery supported；request-local `ReferenceShadow` + current imported shape 恢复仍是 `request_local_backend_gap_candidate`，交给 S4 决定是否已有覆盖；跨请求完整 import cache、完整 BREP、TopoDS_Shape、NamedShape 或 ElementMap 持久状态均为 `diagnostic_non_goal`。

继承自 C7-M7 的完整 imported ElementMap、ShowElement persistent writeback、STL complete Part ElementMap 与 cross-document hash / postfix lifecycle 保持 `known_gap_retained`，不在 C8-M7 重开。当前 capability residual 与上述拆分存在 publication drift，S2 路由为 `unexpected_mismatch` 并交给 S5 精确决定移除、保留已命名 known gap，或在 S3/S4 发现 request-local mismatch 时才进入 S6 受限代码闸门。

## S3 import 文件生命周期 oracle 复审结论

S3 未采 native FreeCAD oracle：S2 没有 `oracle_candidate` 行，文件生命周期语义可由 FreeCAD source、current `cad-core` source 和现有 P6 / P8 / adapter tests 裁决。复核结论是，readable same / new `FileName` 已由当前请求重导入覆盖；`FeaturePartImportBrep` / `FeaturePartImportStep` / `FeaturePartImportIges` 与 `TopoShape::import*` 均从当前 `FileName` 读取，`cad-core/src/part/part_import.cpp` 也在每次执行中读请求 `FileName` 并发布 current request 的 import `NamedShape` / `import_shape_element_map`。

deleted / unreadable `FileName` 只收口为显式 diagnostic / `known_gap_retained`：FreeCAD 走 `Cannot open file` 错误返回，`cad-core` 对缺失或非普通文件也发出 `execution_failed` / `Cannot open file` 诊断并把对象置为 error。S3 不声明 deleted-file old geometry recovery supported，不引入跨请求 import cache，也不重开 C7-M7 inherited persistent lifecycle。旧 subshape 是否能在当前 imported shape + request-local `ReferenceShadow` 边界内恢复，仍交给 S4 复审。

## S4 ElementMap 与 ReferenceShadow 恢复边界复审结论

S4 不打开代码闸门。`namedShapeForImportedShape()` 已为当前请求导入 shape 生成 `owner.ElementN` alias、element sources 和 `import_shape_element_map` preserved mapper history；P6 STEP / BREP import tests 覆盖了 owner-qualified Face / Edge alias 与 mapper history。`currentSubshapeForReference()` 先通过 current `NamedShape` 的 `ElementMap` 调 `part::resolveElementReference()`，只有当前 named shape 不能解析时才退到当前 shape 的可见 subshape 查找。

`recoverSubshapeForReference()` 仍是 request-local：它要求 `view.shapes` 中存在当前请求对象，只在当前 `shape` 或当前 `InternalShape` 内调用 `recoverReferenceShadowSubshape()`，并用请求携带的 `ReferenceShadow` fingerprint 或单 subshape BREP 证据做唯一性验证；没有 current shape 时只能返回 missing 并走 diagnostic。`referenceShadowUpdateJson()` 只接收一个已解析的 `currentSubshape`，刷新 fingerprint，并且仅在输入已有 `ReferenceShadow.brep` 时用当前单 subshape snapshot 更新该字段；它不发布完整对象 BREP，也不保存跨请求 `NamedShape` / `ElementMap` / `TopoDS_Shape` cache。

因此 C8-M7 S4 将 `C8M7-SCOPE-201`、`C8M7-SCOPE-202`、`C8M7-SCOPE-203` 收口为 `already_covered`，把 `C8M7-SCOPE-204` 继续保持为 `diagnostic_non_goal`。deleted-file 且没有 current imported shape 的 old geometry recovery 不属于无状态 CAD Core，本包后续 S5 只处理 capability residual 发布口径，S6 只有在 S5 发现发布需要代码闸门时才进入实现。

## S5 capability 残留与 non-goal 发布准入结论

S5 裁决当前 `import_shape.remaining=["changed_file_deleted_reference_recovery"]` 是 stale mixed publication，不是新的 request-local backend gap。S3 已证明 readable changed-file / same-file `FileName` 会按当前请求文件重新导入并发布 current request 的 `NamedShape` / ElementMap / mapper history；S4 已证明 current imported shape + request-local `ReferenceShadow` fingerprint / 单 subshape BREP 证据的恢复边界已覆盖。deleted / unreadable `FileName` 没有 current imported shape 时只能走 explicit diagnostic，不能声明 old geometry recovery supported。

S6 路线因此限定为 capability publication closure：只允许修改 `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py`，将 `import_shape.remaining` 中的 stale mixed token 移除，并用 adapter capability smoke 锁住该 token 不再出现。如果 capability JSON 需要继续表达 deleted-file old geometry recovery，必须以 non-goal / diagnostic wording 表达，不得进入 `covered` 或 supported known-gap。S6 不允许改 runtime import、reference resolution、elementReferenceUpdates、fixture expected、adapter 字符串改写或下游同步。

## S6 发布闸门结论

S6 走 capability publication closure，不打开 runtime backend gate。`topo_history.producer_matrix.import_shape` 继续发布 `status=done_first_slice`，`covered=["step","brep","iges","owner_qualified_alias"]`，`remaining=[]`；`changed_file_deleted_reference_recovery` 不再出现在 capability JSON。

deleted-file old geometry recovery 没有进入 supported / covered。该场景仍按 S3-S5 裁决保持显式 diagnostic、`known_gap_retained` 或 `diagnostic_non_goal`，不得依赖跨请求 cache、完整 BREP、TopoDS_Shape、NamedShape 或 ElementMap。

本轮只修改 `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_adapters.py`、C8-M7 README / 矩阵、`docs/CADCore8.0/README.md` 和 S6 文件名。`C8M7-BLOCKER-601` 关闭，S6 文件重命名后 C8-M7 队列为空。

## 主文件

- 总入口：`6-27-15-37-C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线总入口.md`
- 方案：`6-27-15-37-C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 步骤队列

1. S0：已完成 live 基线与 residual 声明冻结。
2. S1：已完成 FreeCAD source 与 current coverage 复核。
3. S2：已完成准入路由与 blocker 矩阵。
4. S3：已完成 import 文件生命周期 oracle 复审。
5. S4：已完成 ElementMap 与 ReferenceShadow 恢复边界复审。
6. S5：已完成 capability 残留与 non-goal 发布准入，S6 路线为受限 capability publication patch。
7. S6：已完成 capability publication closure 与发布闸门。

## 允许代码落点

S5 未发现 request-local、source-backed、非跨请求缓存的实现缺口，因此 S6 不打开 runtime backend gate。S6 允许代码落点限于：

- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`

S6 可同步回写本包 README / 矩阵 / S6 step 状态。禁止修改 `cad-core/src/part/part_import.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/part/topo_shape.cpp`、`cad-core/src/runtime/reference_resolution.cpp`、`cad-core/src/runtime/element_reference_update.cpp`、fixtures、expected、adapter 输出字符串或下游仓库。禁止用 fixture 名称分支、输出排序猜测、文件名特判、跨请求 shape cache、持久完整 BREP、持久 TopoDS_Shape、持久 NamedShape 或持久 ElementMap 关闭 residual。

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
cmake --build build
./cad-core capabilities > /tmp/c8m7-capabilities.json
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_adapters
```
