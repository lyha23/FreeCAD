# C6-M8 Part Workbench Surface Family Published Contract Closure 主线

本目录承接 C6-M7 之后的 CAD Core 6.0 下一批工作。C6-M1 到 C6-M7 已把 Pipe、Sweep、Filling、GeomPlate 和 Loft 的产品合同或历史边界逐步发布；C6-M8 的目标不是做一层薄审计，而是把 Part Workbench surface family 的公开 capability 合同做一次可执行收口：批量复核 `ProjectOnSurface`、`RuledSurface`、`Loft`、`Sweep`、`Filling`、`GeomPlate` 的 expected-backed、CAD Core product contract non-parity、historical evidence、narrowed gap 和 non-goal 口径，并按同一公开 API / capability 边界补齐代码、fixtures、focused tests、capability/docs 与验收记录。

## 入口

- 主线总入口：`6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线总入口.md`
- 方案：`6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，S0 起点 `HEAD=02798d9ca9`（`02798d9ca9 docs: 新增 C6-M8 表面族合同收口方案`）；S1 起点 `HEAD=f1f32b19c8`（`f1f32b19c8 文档：完成 C6-M8 S0 live 基线冻结`），两次开始时 `git status --short -uall` 均为空；C6-M1 到 C6-M7 `工作步骤细分` 队列均为空。
- S0/S1 已实现：S0 冻结 live baseline；S1 已复核六个 owner 的 FreeCAD source authority、cad-core 落点、checked-in fixtures、product fixtures、diagnostic evidence 和 adapter assertion，当前队列推进到 S2。
- C6-M7 已发布 `part_workbench.loft.remaining_gaps=[]`，并把 `part_loft_subelement_assignment_native_hidden` 保留为 `narrowed_gaps` / historical native-hidden evidence。
- 当前 surface family 中 `ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 已发布为空 active `remaining_gaps`；`project_on_surface` 仍把 `gui_projection_task_panel`、`unverified_advanced_branches` 同时列在 `remaining_gaps` 和 `non_goals`。
- `cad-core/tests/test_adapters.py` 当前断言与 capability 一致：ProjectOnSurface status 为 `supported_expected_backed_published_slice`，两项 overlap 仍同时断言在 `remaining_gaps` / `non_goals`；RuledSurface / Loft / Sweep / Filling / GeomPlate 的 active `remaining_gaps` 断言均为空。
- C6-M8 的第一优先级仍是 S2 裁决并收口这种“active gap 与 non-goal 同时存在”的发布口径冲突；如果 S2 认定某个分支其实是 CAD Core stateless product candidate，S3 必须按同一 DTO/API 批量实现代表场景，而不是只移动文案。
- 本仓库 `cad-core` 是 C++17/CMake core；C6-M8 不引入 Rust 同步包。若后续需要 `opencascade-rs` 或前端 adapter 对齐，应另开包。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线/矩阵/*.tsv
git diff --check -- docs/CADCore6.0/C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线 docs/CADCore6.0/README.md
```
