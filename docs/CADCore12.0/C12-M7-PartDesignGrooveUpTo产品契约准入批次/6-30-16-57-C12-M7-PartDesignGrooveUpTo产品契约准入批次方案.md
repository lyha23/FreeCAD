# C12-M7 PartDesign Groove UpTo 产品契约准入批次方案

## 结论

下一步应做 `C12-M7 PartDesign Groove UpTo 产品契约准入批次`。

这不是 broad PartDesign 重开，也不是 CopyOnChange 直接实现。当前最适合推进的是 `part_design.revolution_groove.narrowed_gaps.partdesign_groove_upto_brepfeat_cut_native_failure`：它已经从 broad deferred 收敛成两个 exact fixtures，并且 capability 明确允许未来包批准 CAD Core non-parity product contract。

## 当前基线

- 创建基线：`11778397bf docs: 关闭 C12-M6 wire/wire 发布闸门`。
- S0 live 基线：`bb69e61a0f docs: 关闭 C12-M7 工作步骤总入口`，起点 worktree clean。
- C12-M1..M6 工作步骤队列均为空。
- `part_design.sub_shape_binder.remaining_gaps=["copy_on_change_full_temporary_document_cache"]` 仍存在，但 C12-M5 已关闭为 `no_code_retained_diagnostic`。
- `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，`remaining_gaps=[]`，C12-M6 已关闭为 `wire_wire_admitted_current_supported`。
- `part_design.revolution_groove.status=supported_c51s1_advanced_with_historical_groove_upto_native_failure`。
- narrowed gap：`partdesign_groove_upto_brepfeat_cut_native_failure`，fixtures 为 `c51m1/partdesign-groove-uptofirst-body` 和 `c51m1/partdesign-groove-uptoface-body`。

## FreeCAD / CAD Core 依据

- FreeCAD source：`src/Mod/PartDesign/App/FeatureGroove.cpp::Groove::execute()` 调用 subtractive revolved path。
- FreeCAD source：`src/Mod/PartDesign/App/FeatureRevolved.cpp::Revolved::tryExecuteRevolved()` 对 ToFirst / ToFace / ToLast 进入 `BRepFeat_MakeRevol` up-to path。
- FreeCAD source：`src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementRevolution()` 是 BRepFeat revolution helper 落点。
- current CAD Core：`cad-core/src/part_design/feature_revolved.cpp::buildRevolvedUntil()` 对 UpTo revolution 进入 `makeElementRevolutionUntilFromSources()`。
- current CAD Core：`cad-core/src/part/topo_shape_expansion.cpp` 当前 exact diagnostic 为 `BRepFeat_MakeRevol could not revolve profile up to face`。
- current tests：`cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers()` 已断言两个 Groove UpTo fixtures 的 exact diagnostic。
- current capability：`cad-core/tests/test_adapters.py` 已断言该 narrowed gap 不是 `remaining_gaps`，而是 historical native failure。

## 最小完整语义批次

必须同时覆盖 `Groove Type=UpToFirst` 和 `Groove Type=UpToFace`，不能只挑一个 fixture。两者共享同一 FreeCAD BRepFeat cut path、同一 CAD Core subtractive replay边界、同一 current diagnostic 和同一 capability narrowed gap。

本包要一次性裁决：

- native failure 是否仍成立。
- current diagnostic 是否稳定且可定位。
- 是否允许把 exact diagnostic 作为 product diagnostic contract。
- 若允许，expected/test/capability/docs 应如何一起迁移。
- 若不允许，保留 historical native failure 的重开条件。

## 预期实现面

若 S2 批准 product diagnostic contract，S3 可修改：

- `cad-core/fixtures/c51m1/expected/`：新增或更新 Groove UpTo diagnostic product-contract expected。
- `cad-core/tests/test_p7_features.py`：让两个 fixtures 使用 expected-backed diagnostic contract，而不是只靠手写断言。
- `cad-core/src/runtime/capability_contract.cpp`：把 Groove UpTo narrowed gap wording 从 historical native failure 调整为 approved product diagnostic contract，保留 native failure note。
- `cad-core/tests/test_adapters.py`：同步 capability assertion。
- `docs/CADCore12.0/README.md` 与本包矩阵：记录 C12-M7 最终出口。

除非 S1 证明 FreeCAD native baseline 已成功且 S2/S3 证明 current mismatch，否则不改：

- `cad-core/src/part_design/feature_revolved.cpp`
- `cad-core/src/part/topo_shape_expansion.cpp`
- `cad-core/include`
- geometry builder 语义

## 非目标

- 不实现 full `Groove` / `Revolution` 家族。
- 不把 FreeCAD native failure 改写成 FreeCAD parity success。
- 不用 output order、bbox、fixture 名称或 adapter logic 伪造 target face / source relation。
- 不重开 SubShapeBinder CopyOnChange。
- 不重开 RuledSurface wire/wire。
- 不处理 Datum AttachEngine、Pipe、Filling、GeomPlate 或 ProjectOnSurface。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
git diff --check
```

实现步 focused tests：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
```
