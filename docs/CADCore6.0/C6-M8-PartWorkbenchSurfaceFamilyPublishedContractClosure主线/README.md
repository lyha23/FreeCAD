# C6-M8 Part Workbench Surface Family Published Contract Closure 主线

本目录承接 C6-M7 之后的 CAD Core 6.0 下一批工作。C6-M1 到 C6-M7 已把 Pipe、Sweep、Filling、GeomPlate 和 Loft 的产品合同或历史边界逐步发布；C6-M8 的目标不是做一层薄审计，而是把 Part Workbench surface family 的公开 capability 合同做一次可执行收口：批量复核 `ProjectOnSurface`、`RuledSurface`、`Loft`、`Sweep`、`Filling`、`GeomPlate` 的 expected-backed、CAD Core product contract non-parity、historical evidence、narrowed gap 和 non-goal 口径，并按同一公开 API / capability 边界补齐代码、fixtures、focused tests、capability/docs 与验收记录。

## 入口

- 主线总入口：`6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure主线总入口.md`
- 方案：`6-25-10-53-C6-M8-PartWorkbenchSurfaceFamilyPublishedContractClosure方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- live 起点：`pwd=/Users/li/Chili3DProject/FreeCAD`，S0 起点 `HEAD=02798d9ca9`（`02798d9ca9 docs: 新增 C6-M8 表面族合同收口方案`）；S1 起点 `HEAD=f1f32b19c8`（`f1f32b19c8 文档：完成 C6-M8 S0 live 基线冻结`）；S2 起点 `HEAD=f981730d1c`（`f981730d1c 文档：完成 C6-M8 S1 源码与矩阵复核`）；S3 起点 `HEAD=72f173f682`（`72f173f682 文档：完成 C6-M8 S2 ProjectOnSurface 路由裁决`）；S4 起点 `HEAD=d4a96dea91`（`d4a96dea91 docs: 完成 C6-M8 S3 发布口径收口`），五次开始时 `git status --short -uall` 均为空；C6-M1 到 C6-M7 `工作步骤细分` 队列均为空。
- S0/S1/S2/S3/S4 已实现：S0 冻结 live baseline；S1 已复核六个 owner 的 FreeCAD source authority、cad-core 落点、checked-in fixtures、product fixtures、diagnostic evidence 和 adapter assertion；S2 已裁决 ProjectOnSurface overlap；S3 已按 S2 route 收口 capability + adapter assertion + docs/matrices；S4 已完成 publication docs/matrix/root README 一致性收口，当前队列推进到 S5。
- C6-M7 已发布 `part_workbench.loft.remaining_gaps=[]`，并把 `part_loft_subelement_assignment_native_hidden` 保留为 `narrowed_gaps` / historical native-hidden evidence。
- 当前 surface family 中 `ruled_surface`、`loft`、`sweep`、`filling`、`geomplate` 已发布为空 active `remaining_gaps`，S2 复核确认它们与 fixtures、`narrowed_gaps`、`non_goals` 一致。
- S2 裁决：`project_on_surface.gui_projection_task_panel` 是 `non_goal_frozen`；`project_on_surface.unverified_advanced_branches` 已拆为 `expected_backed_closed` stateless DTO/API、`non_goal_frozen` GUI/session、`historical_narrowed_gap` native mapper hidden evidence、`non_goal_frozen` broad placeholder。
- `cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 已在 S3 中发布 S2 route：`part_workbench.project_on_surface.remaining_gaps=[]`，GUI/session 只保留在 `non_goals`，native mapper hidden 作为 `narrowed_gaps` / request-local historical evidence，宽泛 `unverified_advanced_branches` 不再进入 active gap 或 non-goal；S3 未新增 C++ executor、fixtures 或 expected。
- C6-M8 的下一优先级是 S5 release gate：阶段回归确认 ProjectOnSurface S3/S4 发布口径与 RuledSurface / Loft / Sweep / Filling / GeomPlate 的 `remaining_gaps=[]`、`narrowed_gaps`、`non_goals` 文档和 adapter assertions 一致。
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
