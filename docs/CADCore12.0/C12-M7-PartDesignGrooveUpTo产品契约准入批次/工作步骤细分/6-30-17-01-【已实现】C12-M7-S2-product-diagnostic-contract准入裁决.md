# 【已实现】C12-M7 S2 product diagnostic contract 准入裁决

## 目标

决定 Groove UpTo current exact diagnostic 是否可以作为 CAD Core product diagnostic contract 发布。

## 必读来源

- S0 / S1 已实现文档
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers`
- `cad-core/tests/test_adapters.py` 的 `part_design.revolution_groove` capability assertion
- 本包 `c12m7_groove_upto_contract_matrix.tsv`

## 操作

1. 区分三种状态：FreeCAD parity success、historical native failure、CAD Core product diagnostic contract。
2. 若 native 仍失败，检查 current diagnostic 是否足够稳定、locatable、request-local，并能作为产品可见行为。
3. 若批准 product contract，写清 S3 需要改的 expected/test/capability/docs 文件面。
4. 若不批准，保留 `retained_historical_native_failure` 并写清重开条件。

## 裁决规则

- Native 仍失败时，不允许几何 C++ parity 实现。
- Product diagnostic contract 必须同时覆盖 UpToFirst 与 UpToFace。
- Capability wording 必须保留 native failure note，不能写成 FreeCAD parity。

## 本轮基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=cc9e3a1190`。
- `git log -1 --oneline=cc9e3a1190 docs: 完成 C12-M7 S1 证据复核`。
- `git -c core.quotepath=false status --short -uall` 无输出，S2 起点 worktree clean。
- S2 开始前队列首项仍是本文件，S3-S5 pending。

## 必读复核

- S0 / S1 已实现文档确认：FreeCAD native failure 已保留，current diagnostic 未漂移。
- `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers()` 当前同时覆盖 `partdesign-groove-uptofirst-body` 与 `partdesign-groove-uptoface-body`，断言两条 `execution_failed`，primary message 为 `BRepFeat_MakeRevol could not revolve profile up to face`，secondary message 为 `Could not revolve the sketch`。
- `cad-core/tests/test_adapters.py` 当前仍把 `part_design.revolution_groove` 发布为 `supported_c51s1_advanced_with_historical_groove_upto_native_failure`，`remaining_gaps=[]`，并把 `partdesign_groove_upto_brepfeat_cut_native_failure` 放在 `narrowed_gaps`，route 为 `historical_native_failure`。
- 产品诊断先例已复核：`cad-core/fixtures/c5m9/expected/part-project-on-surface-invalid-provenance-diagnostics.freecad.json` 和 `cad-core/fixtures/c6m7/expected/part-loft-subelement-product-invalid.freecad.json` 都明确写成 CAD Core product diagnostic contract，不声称 FreeCAD native parity。

## 裁决结论

批准将 Groove UpTo current exact diagnostic 作为 CAD Core product diagnostic contract 进入 S3 迁移。

三种状态必须保持分离：

- FreeCAD parity success：当前不成立。S1 保留的 native evidence 仍是 `FreeCADCmd 1.2.0 revision 20260519` 下两个 Groove UpTo fixtures 报 `Groove: Revolution: Up to face: Could not revolve the sketch!`，本包不得把该状态写成几何成功或 native parity。
- historical native failure：继续保留为来源说明和能力边界。S3 capability wording 必须说明 native BRepFeat_MakeRevol failure 仍存在，不能删除 native failure note。
- CAD Core product diagnostic contract：本轮批准。CAD Core 对 request-local Groove UpTo failure 发布稳定、可定位、产品可见的 diagnostic contract，而不是发布 FreeCAD 几何 parity。

## 批准依据

- 稳定性：S1 focused test 已通过并确认两个 fixtures 的 primary / secondary diagnostics 未漂移；本轮临时 recompute 也得到相同 primary message。
- locatable：`UpToFirst` primary diagnostic 为 `object=Groove`、`property=Type`、`stage=runtime`、`subname=UpToFirst`；`UpToFace` primary diagnostic 为 `object=Groove`、`property=UpToFace`、`target=Pad`、`subname=Face4`、`stage=runtime`。
- request-local：两条 fixtures 的定位字段都来自请求内 `Groove.Type` / `Groove.UpToFace`、`Pad.Face4` 和 `SketchGroove` axis/profile 输入，不依赖跨请求缓存或 native session 状态。
- 产品可见：当前 response 已把 primary diagnostic 暴露在 `diagnostics[]`，`Groove` 为 `error`，`Body` 为 `skipped`；这比只保留 hidden native failure 更适合作为 CAD Core 前端可呈现行为。
- 批量覆盖：批准范围同时覆盖 `c51m1/partdesign-groove-uptofirst-body` 和 `c51m1/partdesign-groove-uptoface-body`，不允许只迁移其中一个 fixture。

## S3 必改面

S3 应按批准口径迁移公开契约，但仍不得改几何 C++：

- 新增或更新 `cad-core/fixtures/c51m1/expected/partdesign-groove-uptofirst-body.freecad.json`。
- 新增或更新 `cad-core/fixtures/c51m1/expected/partdesign-groove-uptoface-body.freecad.json`。
- 更新 `cad-core/tests/test_p7_features.py::test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers()`，从手写 exact blocker 断言迁移为 expected-backed product diagnostic contract assertion，并保留 primary / secondary diagnostic 区分。
- 更新 `cad-core/src/runtime/capability_contract.cpp`，将 wording 从 historical native failure narrowed gap 迁移为 approved CAD Core product diagnostic contract，同时保留 FreeCAD native failure note。
- 更新 `cad-core/tests/test_adapters.py` 的 `part_design.revolution_groove` capability assertion，同步 status、route、expected-backed fixtures、diagnostic wording 和 native failure note。
- 更新 C12-M7 README / 总入口 / 方案 / 矩阵，记录 S3 迁移结果与 S4 focused validation 入口。

## 保留条件

- `retained_historical_native_failure` 作为 native evidence 状态继续保留，但不作为 S2 最终出口。
- 若未来同一 FreeCAD / LibPack / OCCT oracle baseline 显示 Groove UpTo native 成功，且 CAD Core current 与 native stable expected mismatch，才可重开 geometry implementation candidate。
- 若 S3 发现 expected-backed fields 无法同时稳定覆盖 UpToFirst 与 UpToFace，或 capability 无法同时保留 product contract 与 native failure note，则必须停止迁移并回退到 retained historical native failure 口径。

## 输出

- `C12M7-BLOCKER-201` 关闭为 `closed_s2_product_contract_approved`。
- `C12M7-CON-002/003` 关闭为 S2 批准状态；primary diagnostic 是 contract 主体，secondary diagnostic 仅作为 execution context。
- `C12M7-SCOPE-003/004` 关闭为 S2 批准状态；S3 行改为 approved-next-step 口径。
- 本步未修改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests、adapters 或 capability source，未实现几何 C++，未处理 S3-S5。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
git diff --check
```
