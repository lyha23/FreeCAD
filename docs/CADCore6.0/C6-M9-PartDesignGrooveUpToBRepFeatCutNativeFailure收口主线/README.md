# C6-M9 PartDesign Groove UpTo BRepFeat Cut Native Failure 收口主线

本目录承接 C6-M8 之后的 CAD Core 6.0 下一批工作。C6-M8 已把 Part Workbench surface family 的公开 capability 合同收口；当前 live capability 中仍保留的明确实现候选不是 surface family，而是 `part_design.revolution_groove` 下的 native failure evidence：`partdesign_groove_upto_brepfeat_cut_native_failure`。

C6-M9 的目标是围绕 `PartDesign::Groove` 的 `Type=UpToFirst` / `Type=UpToFace` 做一轮可执行收口：先冻结 FreeCAD native failure、cad-core 当前失败、fixtures 和 adapter assertion；再裁决它是应继续作为 historical/native failure evidence，还是可以作为 CAD Core request-local product contract non-parity 实现；若进入实现，必须同时补 cad-core C++、fixtures/product metadata、focused tests、capability/docs 和 release gate。

## 入口

- 主线总入口：`6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线总入口.md`
- 方案：`6-25-12-14-C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 live 起点已冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，当前 `HEAD=17116567e4`（`17116567e4 文档：完成 C6-M8 S5 发布闸门`）。
- S0 执行起点 `git status --short -uall` 只包含根 `docs/CADCore6.0/README.md` 修改和本 C6-M9 包新增文件；未发现 C6-M9 / 根 README 范围外 dirty 文件。
- S1 live 起点已复核：`HEAD=bb03433646`（`bb03433646 文档：冻结 C6-M9 S0 live 基线`），执行起点 `git status --short -uall` 无输出；S1 完成后队列应推进到 S2。
- S3 已发布 `part_design.revolution_groove.status=supported_c51s1_advanced_with_historical_groove_upto_native_failure`。
- S3 已将 `part_design.revolution_groove.remaining_gaps=[]`、`exact_blockers={}` 写入 capability 和 adapter assertion；`partdesign_groove_upto_brepfeat_cut_native_failure` 保留在 `narrowed_gaps` / `field_boundaries.historical_native_evidence`，并绑定 `c51m1/partdesign-groove-uptofirst-body` 与 `c51m1/partdesign-groove-uptoface-body`。
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers` 当前断言两个 Groove UpTo fixture 失败，diagnostic 为 `BRepFeat_MakeRevol could not revolve profile up to face` / `Could not revolve the sketch`。
- S1 已确认 `Groove Type=UpToFirst` 与 `Groove Type=UpToFace` 是同一 subtractive UpTo / `BRepFeat_MakeRevol` 语义批次：前者当前首个 diagnostic 的 property=`Type`、subname=`UpToFirst`，后者 property=`UpToFace`、target=`Pad`、subname=`Face4`；二者都不能在 S2 之前写成 expected-backed success。
- S2 已裁决二者的唯一 route 均为 `historical_native_failure`：它们是 FreeCAD native `BRepFeat_MakeRevol` 稳定失败证据，不进入 `backend_gap_requires_implementation` 或 `cad_core_product_contract_non_parity`。
- `Revolution Type=UpToFirst/UpToLast/UpToFace` 已由 C51 fixtures 支持，C6-M9 不重开 `PartDesign::Revolution` 已支持路径；只处理 `PartDesign::Groove` subtractive UpTo exact blocker。
- S3 已完成 publication/assertion 第一段：不改 C++、fixtures 或 expected；把 `partdesign_groove_upto_brepfeat_cut_native_failure` 从 active `remaining_gaps` 迁出，保留为 `narrowed_gaps` / historical native failure evidence，并同步 adapter assertion。S4 继续做发布一致性复核，S5 做 release gate。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M9-PartDesignGrooveUpToBRepFeatCutNativeFailure收口主线 docs/CADCore6.0/README.md
```
