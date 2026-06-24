# C6-M7 Part Workbench Loft Subelement Assignment Native Hidden 收口主线

本目录承接 C6-M6 之后的 CAD Core 6.0 下一批工作：围绕 S0 冻结时的 `part_workbench.loft.remaining_gaps=["part_loft_subelement_assignment_native_hidden"]` 建立一个可执行的收口包。目标不是重开完整 Loft 或 full Part surface family，而是把该 Loft gap 复核清楚，并按证据收敛为 product contract、diagnostic boundary、narrowed gap 或 non-goal。

## 入口

- 主线总入口：`6-25-00-53-C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线总入口.md`
- 方案：`6-25-00-53-C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 已冻结 live baseline：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=5caad308a9`，`git log -1=5caad308a9 发布 C6-M6 GeomPlate release gate`。
- C6-M1 到 C6-M6 队列均返回空表；S2 完成后 C6-M7 队列从 S3 继续；C6-M6 已发布 `part_workbench.geomplate.remaining_gaps=[]`。
- 当前 CAD Core capability 中，Part Workbench surface family 的 Sweep / Filling / GeomPlate active remaining gaps 已清空；ProjectOnSurface 剩余项为 GUI / unverified advanced branch 边界，已经同时列入 non-goals。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 均确认 `part_workbench.loft.remaining_gaps` 只有 `part_loft_subelement_assignment_native_hidden`。
- C5-M12 已关闭 Loft broad `complex_profile_family`，并保留 `c5m12/part-loft-subelement-assignment-diagnostic` 作为 native-hidden diagnostic-only 证据：`TypeError: Type must be App.DocumentObject or None, not tuple`，未采集 `object_fields.sections[].subname` 和 selected Sketch subelement `shape_summary`。
- S1 已复核 FreeCAD Loft 调用链：`Loft::execute()` 读取 `Sections.getValues()`，逐个对象调用 `getTopoShape(... ResolveLink | Transform)`，再调用 `result.makeElementLoft(...)`；`Sections` 是 `App::PropertyLinkList` object-level link，不是 native `PropertyLinkSubList`。
- S1 结论：当前 gap 的 FreeCAD 侧证据仍是 `PropertyLinkList` native-hidden subelement storage；cad-core 当前只读取 object-level `app::readLinks(object, "Sections")`。S2 已把 selected subelement 支持批准为 request-local product contract non-parity DTO，不能写成 FreeCAD expected parity。
- S2 已批准 C6-M7 走 request-local CAD Core selected subelement product contract 路线：route decision 为 `cad_core_product_contract_non_parity`。FreeCAD `Part::Loft.Sections` 仍是 object-level `App::PropertyLinkList`，C5-M12 diagnostic expected 继续作为 `nativeHidden` / `diagnosticOnly` evidence 保留，S3 不得生成 FreeCAD native expected 或声明 parity。
- S3 已实现 request-local CAD Core Loft selected subelement product contract：`PropertyLinkList.values[]` 可携带 `value/SubList/StableSubList` rich item，Loft 解析 selected section subshape 并发布 `contract_provenance=cad_core_product_contract_non_parity`、`section_entries`、`selected_sections`；invalid subshape 给 `invalid_subshape`。
- S3 新增 `cad-core/fixtures/c6m7/part-loft-subelement-product*.json` 和 expected/product metadata，均声明不是 FreeCAD native parity；C5-M12 diagnostic expected 保持不变。
- S4 已把 S3 结果发布到 `part_workbench.loft` capability 和 adapter assertion：新增 C6-M7 product fixtures、request-local product-contract evidence、`cad_core_product_contract_non_parity` provenance，`remaining_gaps=[]`，并把 `part_loft_subelement_assignment_native_hidden` 转入 `narrowed_gaps` / historical native-hidden evidence。
- S4 仍保留 C5-M12 diagnostic expected 作为非 parity / historical evidence；FreeCAD native selected subelement expected、PartDesign Loft 和 full Part surface family 仍是非目标。
- C6-M7 只处理 `Part::Loft.Sections` 的 subelement assignment 证据闭环，不声明 FreeCAD parity、PartDesign Loft、GUI Loft 或 full Part surface family；S5 下一步是阶段回归和 release gate。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```
