# C12-M18 CAD Core live backlog re-audit 批次

C12-M18 是 C12-M17 关闭后的 live backlog 复审包。它不预设下一个 C++ implementation 目标，而是重新从当前 `cad-core` capability、C12-M1..M17 release gate、`narrowed_gaps`、非原生产品扩展边界和当前测试面中筛选是否存在新的可实现项。

当前 live 结论是：C12-M17 队列已关闭，当前 capability 中没有非空 `remaining_gaps` 或 `known_gaps`。仍存在的 `narrowed_gaps` 是历史 native-hidden、helper-blocked、oracle-blocked、product-contract non-parity 或 current-covered 记录；S2 三闸门复审未发现 `mismatch_confirmed` 行，不能直接升级为实现包。S3 已确认 PartDesign 几何共线 BSpline / 非 Line 轴引用继续作为 product extension 保留，C12-M11 / C12-M15 / C12-M16 sketch token 后端状态 current-supported，剩余消费问题只作为 `my-chili3d` frontend sync candidate。S4 已关闭 next package authorization：不授权 FreeCAD/cad-core C++ implementation package，不创建 oracle/product-contract package；唯一后续建议是外部 `my-chili3d` frontend sync package，S5 只做发布闸门和 FreeCAD/cad-core no-code 收口口径。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=14bbd0ceb9`（`14bbd0ceb9 feat: 修复 SubtractivePipe product PipeLaw 主 Shape 生命周期`）。
- 创建时 worktree clean。
- S0 live 冻结：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=e29b6351c5`（`e29b6351c5 文档：关闭 C12-M18 工作步骤总入口`）。
- S0 起点 `git -c core.quotepath=false status --short -uall` 输出为空。
- C12-M17 队列只输出 markdown 表头。
- `cad-core/build/cad-core capabilities` 当前没有任何非空 `remaining_gaps`。
- `cad-core/build/cad-core capabilities` 当前没有任何非空 `known_gaps`。
- S1 live 抽取：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=09c7dad8ed`（`09c7dad8ed 文档：冻结 C12-M18 S0 live 基线`），起点 worktree clean。
- S1 regenerated `/tmp/c12m18-capabilities.json`；非空 `remaining_gaps` 与非空 `known_gaps` jq 查询均无输出。
- live `narrowed_gaps` 仍存在于：
  - `part_design.revolution_groove`
  - `part_workbench.filling`
  - `part_workbench.geomplate`
  - `part_workbench.loft`
  - `part_workbench.project_on_surface`
  - `part_workbench.sweep`
- S1 narrowed-gaps key count is 15:
  - `part_design.revolution_groove.narrowed_gaps`: `partdesign_groove_upto_brepfeat_cut_native_failure`
  - `part_workbench.filling.narrowed_gaps`: `filling_non_boundary_support_order_native_helper_blocker`, `filling_params_all_native_helper_blocker`, `filling_params_pts_anisotropy_tol_g1_g2_max_segments_blocker`, `filling_support_order_g1_native_helper_blocker`, `filling_support_order_g2_native_helper_blocker`, `filling_surface_native_helper_blocker`
  - `part_workbench.geomplate.narrowed_gaps`: `curve_constraint_criteria_setters_not_implemented`, `g1_curve_on_surface_native_hidden_diagnostic_only`, `platesurface_curves_wrapper_lifecycle`, `projected_curve2d_no_initial_surface_v1_v2_native_oracle_blocker`
  - `part_workbench.loft.narrowed_gaps`: `part_loft_subelement_assignment_native_hidden`
  - `part_workbench.project_on_surface.narrowed_gaps`: `native_project_on_surface_mapper_history_oracle_unavailable`
  - `part_workbench.sweep.narrowed_gaps`: `part_sweep_advanced_combined_freecadcmd_wrapper_build_blocker`, `part_sweep_located_profile_freecadcmd_wrapper_build_blocker`
- Publication authority is `cad-core/src/runtime/capability_contract.cpp::capabilityContractJson()` plus local capability helpers; focused adapter assertions are in `cad-core/tests/test_adapters.py::CadCoreAdapterTest.test_c_api_capabilities_publication_smoke` and `test_c_api_capabilities_exposes_web_contract_facts`.
- S1 only establishes the input list. It does not judge current mismatch and does not turn `narrowed_gaps` into implementation items.
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 当前只保留 PartDesign 几何共线 BSpline / 非 Line 轴引用产品扩展为当前非原生差异；C12-M17 已整改 SubtractivePipe product PipeLaw 主 `Shape` lifecycle。
- S2 三闸门复审：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=60da049164`（`60da049164 文档：关闭 C12-M18 S1 capability 抽取`），起点 worktree clean。
- S2 已对 Groove、Sweep、Filling、GeomPlate、Loft、ProjectOnSurface、Assembly、Sketch/topo 和 SubShapeBinder family 写明 expected/contract、request-local boundary 与 current comparison status。
- S2 结论：没有 `current_comparison=mismatch_confirmed` 行。Groove 保留 C12-M7 product diagnostic non-parity；Sweep/Filling/GeomPlate/Loft/ProjectOnSurface 保留 helper-blocked、native-hidden、oracle-blocked、product-contract non-parity、non-goal 或 current-covered 分类；Assembly、Sketch/topo、SubShapeBinder 为 request-local current-supported 或外部/非目标边界，均不进入 S4 implementation authorization。
- S3 产品扩展与 frontend sync 分流：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=bcd6569a6c`（`bcd6569a6c 文档：关闭 C12-M18 S2 三闸门复审`），起点 worktree clean。
- S3 结论：`docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 当前只把 PartDesign 几何共线 BSpline / 非 Line 轴引用列为保留的 current non-native product extension；SubtractivePipe product PipeLaw 主 `Shape` lifecycle 已在 C12-M17 后列为已整改。C12-M11 open wire raw `EdgeN`、C12-M15 stable geometry id ledger、C12-M16 split fragment ledger 均为 backend current-supported；若产品侧仍有 token / writeback 消费问题，只记录为 `my-chili3d` frontend package candidate，不创建 FreeCAD/cad-core C++ work。
- S4 next package authorization：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=47e1597901`（`47e1597901 docs: 关闭 C12-M18 S3 分流裁决`），起点 worktree clean。
- S4 结论：live backend classification 中没有 `mismatch_confirmed` 行，`implementation_package_authorized` 不成立；历史 helper-blocked、native-hidden、oracle-blocked、product-contract non-parity 或 current-covered 行没有新的 expected/product-contract package 输入，`oracle_or_product_contract_package_required` 不成立；唯一后续路由是外部 `my-chili3d` frontend sync package recommendation，不改 FreeCAD/cad-core C++、fixtures、expected、tests 或 adapters。

## 批次目标

1. 冻结 C12-M1..M17 已关闭事实，避免从旧记忆或旧矩阵重开已经 current-supported 的项。
2. 结构化抽取当前 capability 中的 `remaining_gaps`、`known_gaps` 和 `narrowed_gaps`。
3. 对每个历史 `narrowed_gaps` family 重新应用三闸门：
   - stable native expected 或 approved product contract 是否存在。
   - request-local boundary 是否成立。
   - current `cad-core` 是否存在 mismatch-confirmed 行。
4. 把 PartDesign BSpline / 非 Line 轴引用明确保留为 product extension，不把它误列为 C++ parity bug。
5. 输出下一步分流：implementation package、oracle/product-contract package、frontend consumer sync，或 no-code backlog gate。

## C++ 授权闸门

只有同时满足以下条件，C12-M18 才能授权后续 implementation package：

- FreeCAD source authority 或已批准 product-contract authority 可追溯到具体文件、类/函数和关键字段。
- 语义能在 CAD Core 无状态 request-local 边界内表达，不依赖 backend session、temporary document cache、full BREP / TopoDS 或 persistent `NamedShape` / `ElementMap` cache。
- 有 stable expected、checked-in expected-backed fixture，或已批准的 product diagnostic / product contract expected。
- current output 与该 expected / contract 有可复现 mismatch，且 mismatch 不是已有 product extension、native-hidden、helper-blocked、oracle-blocked 或 docs wording 可解释的状态。
- 有 focused tests 可以约束通用语义，不依赖 fixture 名、bbox、面积、输出顺序或 adapter 修补。

## 初始分流判断

- 当前 FreeCAD/cad-core 默认不应直接开 C++ implementation 包；live capability 已无 active gap。
- 历史 Part Workbench `narrowed_gaps` 需要先做三闸门复审；helper-blocked、native-hidden 和 product-contract non-parity 本身不能写成 supported，也不能直接当作 current mismatch。
- `PartDesign axis accepts geometrically linear BSpline / non-Line curves` 是用户明确保留的 product extension。后续只需保持 capability / expected / roadmap 口径，不应作为 C12-M18 修复项。
- C12-M11 / C12-M15 / C12-M16 留下的前端 consumer sync 属于 `my-chili3d` 外部包；如果本轮发现可见收益优先级更高，应输出 frontend package 建议，而不是在 FreeCAD/cad-core 内发明 backend work。

## 工作步骤

- 入口：已关闭；确认包结构、队列顺序和矩阵字段，后续队列从 S0 开始。
- S0：已关闭；冻结 live HEAD、dirty boundary、C12-M17 队列闭合状态和 C12-M17 后的 capability 空 gap 事实。
- S1：已关闭；抽取 current `remaining_gaps`、`known_gaps`、`narrowed_gaps` path/key、非原生产品扩展和 publication authority，只建立输入清单。
- S2：已关闭；对历史 `narrowed_gaps` family 与 Assembly / Sketch-topo / SubShapeBinder 做 stable expected / product contract、request-local boundary、current mismatch 三闸门复审，未产生 mismatch-confirmed 行。
- S3：已关闭；保留 axis extension，分离 my-chili3d consumer work，不把前端缺口误写成后端 C++ gap。
- S4：已关闭；无 `mismatch_confirmed` 行，不授权 implementation package；无新的 expected/product-contract package 输入；输出外部 `my-chili3d` frontend sync package recommendation。
- S5：发布闸门，更新 root README、矩阵和队列状态。

## 入口

- 总入口：`7-5-12-29-C12-M18-CADCoreLiveBacklogReAudit批次总入口.md`
- 方案：`7-5-12-29-C12-M18-CADCoreLiveBacklogReAudit批次方案.md`
- 工作步骤：`工作步骤细分/`；入口文件已重命名为 `7-5-12-29-【已实现】C12-M18工作步骤总入口.md`。
- 矩阵：`矩阵/`

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M18-CADCoreLiveBacklogReAudit批次 docs/CADCore12.0/README.md
git diff --check
```

候选复核：

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities > /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "remaining_gaps") and ((getpath($p)|type)=="array") and ((getpath($p)|length)>0)) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select(($p[-1]? == "known_gaps") and (((getpath($p)|type)=="array" and (getpath($p)|length)>0) or ((getpath($p)|type)=="object" and (getpath($p)|length)>0))) | {path:($p|join(".")), value:getpath($p)}' /tmp/c12m18-capabilities.json
jq -c 'paths as $p | select($p[-1]? == "narrowed_gaps") | {path:($p|join(".")), keys:(getpath($p)|keys)}' /tmp/c12m18-capabilities.json
```

重型收口只在 S4/S5 授权 implementation package 且实际修改 `cad-core/src`、fixtures、expected 或 shared runtime surface 后执行；开包本身不跑 full FreeCAD build 或全量 CI。
