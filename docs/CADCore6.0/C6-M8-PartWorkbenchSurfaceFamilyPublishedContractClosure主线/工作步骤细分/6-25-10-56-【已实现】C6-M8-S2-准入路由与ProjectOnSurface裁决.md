# 【已实现】C6-M8 S2 准入路由与 ProjectOnSurface 裁决

## 目标

把 S1 的 candidates 路由成可执行状态。S2 是 C6-M8 的关键裁决点：不能把 ProjectOnSurface active/non-goal overlap 留给后续实现者猜。

## S2 live baseline

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`f981730d1c`
- `git log -1 --oneline`：`f981730d1c 文档：完成 C6-M8 S1 源码与矩阵复核`
- `git -c core.quotepath=false status --short -uall`：空输出，S2 开始时工作区干净。
- `step_goal_queue.py` 刷新后当前队列从本 S2 文件开始。

## 路由词典

| route | 含义 | 后续 |
| --- | --- | --- |
| `expected_backed_closed` | 已有 FreeCAD expected / focused tests 支撑，发布口径只需保护。 | S4 publication guard |
| `cad_core_product_contract_non_parity` | CAD Core stateless 产品合同成立，但不是 FreeCAD native expected。 | S3 批量实现或补 fixture/test |
| `historical_narrowed_gap` | 有 native-hidden / crash / timeout / notCollected 证据，应保留 delete condition。 | S4 capability/docs |
| `non_goal_frozen` | GUI、session、persistent wrapper、full parity 等不属于 CAD Core。 | S4 移出 active gap |
| `backend_gap_requires_implementation` | 产品合同清楚且 cad-core 未实现。 | S3 code + fixtures + tests |

## FreeCAD 依据

- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::ProjectOnSurface::tryExecute()` 只读取 `Mode`、`Height`、`Offset`、`Direction`、`SupportFace`、`Projection`，然后调用 `getSupportFace()`、`getProjectionShapes()`、`createProjectedWire()`、`filterShapes()` 和 `createCompound()`。
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp::getViewProviderName()` 返回 `PartGui::ViewProviderProjectOnSurface`，GUI 编辑入口不在 App executor 主路径。
- `src/Mod/Part/Gui/ViewProviderProjectOnSurface.cpp::setEdit()` 通过 `Gui::Control().showDialog(new TaskProjectOnSurface(feature))` 打开 TaskPanel。
- `src/Mod/Part/Gui/DlgProjectionOnSurface.cpp::DlgProjectOnSurface` 是 `Gui::SelectionObserver`，其 `onSelectionChanged()`、`setSelectionGate()`、`onGetCurrentCamDirClicked()`、`onAddFaceClicked()`、`onAddWireClicked()`、`onAddEdgeClicked()` 等入口依赖 GUI selection / camera / TaskDialog session；这些只负责把用户交互写回 App 属性，不是无状态 CAD Core DTO。

## ProjectOnSurface 裁决

| item | route | 理由 | reopen/delete condition | S3/S4 收口 |
| --- | --- | --- | --- | --- |
| `project_on_surface.gui_projection_task_panel` | `non_goal_frozen` | GUI TaskPanel / ViewProvider / selection session 由 `src/Mod/Part/Gui` 承担；CAD Core 后端只接收已经序列化的 `Mode/Height/Offset/Direction/SupportFace/Projection` 图数据。 | 仅当前端/GUI 集成包需要复刻 FreeCAD TaskPanel UX，或产品批准新的 request-local GUI workflow DTO 时重开；在 C6-M8 中从 active `remaining_gaps` 删除。 | S3/S4 同步 capability/test/docs，把它保留在 `non_goals`，不得作为 backend gap。 |
| `project_on_surface.unverified_advanced_branches.stateless_dto_api` | `expected_backed_closed` | App executor 的 stateless 主路径已由 C4-M1/C5-M9 fixtures 覆盖：Edges/Faces/All、Height、Offset、多 Projection 顺序、face rebuild、hole wires、height prism、provenance diagnostics。 | 若未来发现 `tryExecute()` 中新的 App 属性或未覆盖投影模式，另开 product/expected fixture；不能用 broad placeholder 重开。 | S3/S4 保护现有 fixture/assertion，不新增 C++、fixtures 或 expected。 |
| `project_on_surface.unverified_advanced_branches.gui_selection_camera_session` | `non_goal_frozen` | `DlgProjectOnSurface` 的 selection gate、当前相机方向、按钮回调和 TaskDialog 生命周期是 GUI/session 交互；它们最终只写入已有 App 属性。 | 只在 GUI/front-end interaction 包中重开；后端重算接口不保存 session。 | S3/S4 不发布为 active gap；必要时归入 ProjectOnSurface GUI non-goal 文案。 |
| `project_on_surface.unverified_advanced_branches.native_mapper_history_hidden_until_probe` | `historical_narrowed_gap` | 当前 capability 已把 `native_project_on_surface_mapper_history_hidden_until_probe` 放在 request-local boundary；C5-M9 以 projection item ledger 发布 CAD Core provenance，不能把 native hidden mapper/history 伪装成 expected-backed。 | 只有在 FreeCADCmd / native oracle 可稳定导出 ProjectOnSurface mapper/history 与 element ownership 时，才能删除 historical boundary 并升级 expected。 | S3/S4 写入 `narrowed_gaps` / historical evidence，不进入 active `remaining_gaps`。 |
| `project_on_surface.unverified_advanced_branches.broad_placeholder` | `non_goal_frozen` | `unverified_advanced_branches` 是不可执行的宽泛占位，未绑定具体 DTO/API、fixture 或 FreeCAD 调用分支；S2 已拆成上面三个可验证子项。 | S4 后删除该 broad placeholder；以后只能以具体字段、source path、fixture 或 historical evidence 重开。 | S3/S4 用具体 route 替换 broad 字符串，不能继续保留双重 active/non-goal 状态。 |

S2 未发现 `backend_gap_requires_implementation`。因此 S3 的最小完整收口不是 code/fixture 批次，而是 publication/assertion 批次：更新 `cad-core/src/runtime/capability_contract.cpp`、`cad-core/tests/test_adapters.py`、C6-M8 docs/matrices，使 ProjectOnSurface 不再把同一项同时放在 `remaining_gaps` 和 `non_goals`；保留现有 expected-backed fixtures，不改 C++ executor、不新增 expected。

## 其它 surface family 复核

| family | S2 结论 | 后续 |
| --- | --- | --- |
| `ruled_surface` | `remaining_gaps=[]` 与 p8/c4m1 expected-backed fixtures、`non_goals=[]` 一致。 | S4 只做 publication guard。 |
| `loft` | `remaining_gaps=[]` 与 C6-M7 selected subelement product contract、`part_loft_subelement_assignment_native_hidden` narrowed/historical evidence、non-goals 一致。 | S4 保持 native-hidden 不写成 expected-backed。 |
| `sweep` | `remaining_gaps=[]` 与 C6-M4 located profile / advanced combined product contract non-parity、FreeCADCmd wrapper historical blockers、non-goals 一致。 | S4 保持 product contract 与 wrapper evidence 分离。 |
| `filling` | `remaining_gaps=[]` 与 C6-M5 Surface/Supports/Orders/params/non-boundary product contract non-parity、native helper crash/timeout evidence、non-goals 一致。 | S4 保持 helper historical evidence，不声明 native FilledFace DocumentObject。 |
| `geomplate` | `remaining_gaps=[]` 与 C6-M6 G1 CurveOnSurface / ProjectedCurve2d product contract、criteria diagnostic、PlateSurface.Curves non-goal 一致。 | S4 保持 diagnostic/non-goal/historical boundary 分离。 |

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'gui_projection_task_panel|unverified_advanced_branches|pendingRoute|backend_gap_requires_implementation|non_goal_frozen|historical_narrowed_gap|cad_core_product_contract_non_parity|remaining_gaps|non_goals|narrowed_gaps' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
```

## 通过条件

- 每个 active gap / non-goal overlap 都有唯一 route。
- 不存在 S2 新增的 `backend_gap_requires_implementation`。
- S3/S4 范围限定为 capability/test/docs publication assertion：移除 ProjectOnSurface overlap、保留 GUI non-goal、把 native mapper hidden 记录为 historical narrowed boundary。
- S2 文件名和标题已标记为 `【已实现】`，队列推进到 S3。
