# C8-M7 ImportShape changed-file / deleted-reference recovery 准入收口主线总入口

## 结论

C8-M7 是 C8-M6 之后的下一轮 CAD Core 收口包。当前 C8-M1 到 C8-M6 队列已经清空，live capability 中剩余的高信号行不是 ShapeBinder/SubShapeBinder，而是 `topo_history.producer_matrix.import_shape.remaining=["changed_file_deleted_reference_recovery"]`。

本包的首要任务不是立刻写代码，而是确认这条 residual 的真实含义：

- 如果 changed-file 只是同一请求内重新读取当前 `FileName` 并重建 import shape，当前 `cad-core` 可能已经覆盖，应转为 capability publication closure。
- 如果 deleted-reference 需要在文件丢失后从上次请求保存的完整 shape / NamedShape / ElementMap 里恢复，必须保持 non-goal 或 oracle-blocked，因为它违反无状态 CAD Core 边界。
- 如果存在可由请求携带的单 subshape `ReferenceShadow.brep`、stable subname、current file import 和 ElementMap 共同完成的 request-local 恢复缺口，才能进入受限 C++ 实现。

## 上游状态

- `cad-core/src/part/part_import.cpp` 已实现 `ImportBrep`、`ImportStep`、`ImportIges`，均从 `FileName` 读取当前文件，调用 OCCT reader 并通过 `namedShapeForImportedShape()` 发布 import `NamedShape`。
- `cad-core/src/part/topo_shape_expansion.cpp` 已发布 `import_shape_element_map` mapper history，并为 `owner.ElementN` 建立 owner-qualified alias。
- `cad-core/src/runtime/reference_resolution.cpp` 与 `element_reference_update.cpp` 已存在 request-local `ReferenceShadow` 验证、恢复和更新建议，但不保存跨请求完整 shape。
- 当前 capability 仍把 `changed_file_deleted_reference_recovery` 放在 `import_shape.remaining`，因此需要 C8-M7 判断它是实现缺口、发布漂移，还是应被降级为 non-goal / oracle-blocked。
- C7-M7 已证明完整 imported ElementMap / LinkElement 持久写回 / cross-document hash lifecycle 缺可固化 native oracle 或不是无状态后端职责；C8-M7 不重开这些关闭结论。

## 初始范围

- STEP / BREP / IGES `FileName` 当前文件重导入和 import ElementMap / mapper history 发布边界。
- 文件变化后已有外部引用的 stable subname / owner-qualified alias / `ReferenceShadow` request-local 恢复路径。
- 文件删除或不可读时的 diagnostic、non-goal、oracle-blocked 和 capability publication。
- `topo_history.producer_matrix.import_shape` 的 `covered` / `remaining` / `known_gap` 发布口径。

## 排除项

- 不保存跨请求 full BREP、TopoDS_Shape、NamedShape、ElementMap 或 geometry cache。
- 不把 `ReferenceShadow.brep` 扩展成完整对象 BREP transport；它仍只能是单个旧 subshape snapshot。
- 不重开 C7-M7 的 ShowElement 持久写回、完整 Link 账本、cross-document hash / postfix 生命周期或 STL complete ElementMap。
- 不修改 GUI、ViewProvider、Workbench、Worker、WASM、Web 或前端状态同步协议。
- 不用 current `cad-core` 输出倒推 FreeCAD expected。

## 步骤队列

1. S0：已完成 live baseline、C8-M1..M6 empty queue、current capability residual 和 C7-M7 non-goal 继承边界冻结。
2. S1：复核 FreeCAD Part import、TopoShape import、PropertyFile / PropertyLinks / ElementMap source authority，以及 current cad-core coverage。
3. S2：把 changed-file、deleted-file、request-local ReferenceShadow、full persistent cache 和 capability drift 拆入准入矩阵。
4. S3：复审 import 文件生命周期 oracle：当前文件重导入是否已 covered，删除文件是否只能 diagnostic / non-goal。
5. S4：复审 ElementMap 与 ReferenceShadow 恢复边界：只允许 request-local stable subname、single-subshape BREP evidence 和 current shape search。
6. S5：裁决 capability residual：关闭、降级为 known_gap / non-goal，或打开 S6 code gate。
7. S6：执行实现或 no-code 发布闸门，清空队列并更新 CADCore8.0 README。

## 验收入口

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线 docs/CADCore8.0/README.md
git diff --check
```
