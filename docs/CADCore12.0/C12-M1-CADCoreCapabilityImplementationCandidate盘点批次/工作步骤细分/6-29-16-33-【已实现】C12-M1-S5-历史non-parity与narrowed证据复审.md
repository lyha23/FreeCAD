# 【已实现】C12-M1 S5 历史 non-parity 与 narrowed 证据复审

## 目标

复审 Part Workbench historical narrowed / product-contract non-parity 行：Sweep、Filling、GeomPlate、Loft、ProjectOnSurface。S5 只判断这些 retained evidence 是否出现新的 stable oracle / current mismatch；不能把历史 `notCollected`、native-hidden 或 helper blocker 直接升级为 backend gap。

## 输入

- `docs/CADCore11.0/README.md`
- `docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/`
- `docs/CADCore11.0/C11-M2-PartWorkbenchFillingNativeHelperParity复开批次/`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/src/part/part_sweep.cpp`
- `cad-core/src/part/part_filling.cpp`
- `cad-core/src/part/part_geomplate.cpp`
- `cad-core/src/part/part_loft.cpp`
- `cad-core/src/part/part_project_on_surface.cpp`
- `src/Mod/Part/App/PartFeatures.cpp`
- `src/Mod/Part/App/AppPartPy.cpp`
- `src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp`
- `src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp`
- `src/Mod/Part/App/FeatureProjectOnSurface.cpp`

## 范围

1. Sweep：确认 C11-M1 `Location` overload 仍 no-code retained non-parity，`remaining_gaps=[]`。
2. Filling：确认 C11-M2 helper cases 仍 no-code retained non-parity，`remaining_gaps=[]`。
3. GeomPlate：复核 native-hidden / wrapper-lifecycle / product-contract rows 是否仍没有 stable native expected。
4. Loft：复核 selected subelement assignment native hidden row 是否仍只是 narrowed evidence。
5. ProjectOnSurface：复核 mapper history hidden row是否只是 probe-only narrowed evidence。
6. 如果某行已有 stable expected/current mismatch，S5 必须写入 S6 code authorization 候选；否则保持 no-code retained。

## 必须回写的矩阵行

- `C12M1-SCOPE-301..305`
- `C12M1-BLOCKER-501`
- `C12M1-CAT-003`
- `C12M1-NG-004..008`
- `C12M1-VAL-501..506`

## S5 结论

S5 起点确认：

```text
pwd=/home/user/Chili3DProject/FreeCAD
HEAD=39e8fcd03d
git log -1 --oneline=39e8fcd03d docs: 完成 C12-M1 S4 Assembly 产品边界复审
git -c core.quotepath=false status --short -uall=<clean>
```

复审后没有合格 S6 code authorization 候选。五个 historical / narrowed 行均未同时满足 stable native/request-local expected、current cad-core mismatch 和产品边界：

| row | 最终分类 | S6 输入 |
| --- | --- | --- |
| `C12M1-SCOPE-301` Sweep located / advanced combined | `closed_s5_no_code_retained_non_parity` | C11-M1 S6 已关闭为 no-code retained；FreeCADCmd Location overload 仍是 `notCollected` / `OCCError: NCollection_Array1::Value`，capability/tests 保持 narrowed blockers 且 `remaining_gaps=[]`。 |
| `C12M1-SCOPE-302` Filling native helper | `closed_s5_no_code_retained_non_parity` | C11-M2 S6 已关闭为 no-code retained；Surface、Supports/Orders、explicit params、non-boundary support/order 仍没有 stable helper `shape_summary`，direct wrapper 只是 diagnostic control。 |
| `C12M1-SCOPE-303` GeomPlate | `closed_s5_probe_only_retained_narrowed_evidence` | G1 curve-on-surface、ProjectedCurve2d no InitialSurface、CurveConstraint criteria setters、PlateSurface.Curves 仍是 native-hidden / wrapper-lifecycle / historical narrowed evidence；`remaining_gaps=[]`。 |
| `C12M1-SCOPE-304` Loft selected subelement | `closed_s5_native_hidden_retained_narrowed_evidence` | `Loft::execute()` 仍只消费 `Sections.getValues()` object links；selected subelement 是 C6-M7 product contract non-parity，C5-M12 tuple TypeError 只是 native-hidden evidence。 |
| `C12M1-SCOPE-305` ProjectOnSurface mapper history | `closed_s5_probe_only_retained_narrowed_evidence` | 已发布 expected-backed slice current-covered；native mapper/history ownership 仍 hidden until stable probe，current projection item ledger 不是 native expected mismatch。 |

因此 `C12M1-BLOCKER-501` 关闭为 `closed_s5_all_historical_narrowed_rows_retained_no_code_authorization_candidate`，`C12M1-CAT-003` 关闭为 `closed_s5_no_code_retained`。`C12M1-NG-004..008` 已同步收口 GUI / TaskPanel、historical notCollected、adapter shortcut、App::Link overclaim 和 C11 closed-line auto-reopen 禁令。

S5 没有重做 probe、没有运行 FreeCADCmd、没有修改 Part Workbench product contract / capability status、没有改 C++ / tests / fixtures / expected。若未来要重开任一行，必须先取得 stable native/request-local expected，再证明 current cad-core mismatch；禁止 fixture-name branch、geometry-type guessing、bbox/area/order matching、adapter-layer business logic、cross-request shape/BREP/wrapper state 或删除未解释的 historical evidence。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n "part_sweep_located_profile_freecadcmd_wrapper_build_blocker|part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker|filling_surface_native_helper_blocker|filling_support_order_g1_native_helper_blocker|part_loft_subelement_assignment_native_hidden|native_project_on_surface_mapper_history_hidden_until_probe|remaining_gaps" cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py docs/CADCore11.0
rg -n "Sweep::execute|Loft::execute|makeFilledFace|BRepOffsetAPI_MakeFillingPy|BuildPlateSurfacePy|ProjectOnSurface::execute" src/Mod/Part/App/PartFeatures.cpp src/Mod/Part/App/AppPartPy.cpp src/Mod/Part/App/BRepOffsetAPI_MakeFillingPyImp.cpp src/Mod/Part/App/GeomPlate/BuildPlateSurfacePyImp.cpp src/Mod/Part/App/FeatureProjectOnSurface.cpp
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次 docs/CADCore12.0/README.md
git diff --check
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
```

通过条件：

- S5 明确列出每个 historical/narrowed 行的最终分类。
- 没有 stable expected/current mismatch 的行不得进入 C++。
- 若有 candidate，必须包含 exact source authority、fixture/test route、code landing 和 shortcut 禁令。
- 验证后本文件已重命名为 `6-29-16-33-【已实现】C12-M1-S5-历史non-parity与narrowed证据复审.md`，并更新工作步骤索引。

## 非目标

- 不重做 C11-M1 / C11-M2 probe，除非 S5 发现已有 probe 产物不足以分类。
- 不用 crash、timeout、notCollected 推导业务语义。
- 不改 Part Workbench product contract 或 capability status。
