# C12-M18 CAD Core live backlog re-audit 批次方案

## 目标

C12-M18 回答“C12-M17 之后 FreeCAD/cad-core 还应该实现什么”。本包不从历史 `narrowed_gaps` 名称直接推出 C++ work，而是重新用 live capability 和三闸门筛选下一步：implementation、oracle/product-contract、frontend sync，或 no-code backlog gate。

## 当前判断

创建时 live repo 显示：

- C12-M17 已关闭为 `implemented_freecad_main_shape_parity_product_law_retained`。
- 当前 capability 无非空 `remaining_gaps`。
- 当前 capability 无非空 `known_gaps`。
- 仍存在的 `narrowed_gaps` 只表示历史 native-hidden、helper-blocked、oracle-blocked、product-contract non-parity 或 current-covered 记录。
- 当前 `non_native_parity` 文档只保留 PartDesign 几何共线 BSpline / 非 Line 轴引用产品扩展；用户已明确选择支持该行为。

因此，本包默认不授权 C++。只有 S2/S4 证明 stable expected 或 approved product contract、request-local boundary 与 current mismatch 同时成立，才创建后续 implementation package。

## Final status

S5 发布 `frontend_sync_package_recommended` + backend `no_code_backlog_gate`：

- `implementation_package_authorized=false`：S2/S4 没有 `mismatch_confirmed` 后端行。
- `oracle_or_product_contract_package_required=false`：没有新的 stable expected 或 approved product-contract 输入候选。
- `frontend_sync_package_recommended=true`：若产品侧仍有 sketch token / writeback 消费问题，应在外部 `my-chili3d` 建包处理。
- backend `no_code_backlog_gate=published`：FreeCAD/cad-core 不改 C++、fixtures、expected、tests 或 adapters；后端重开条件仍是 stable expected / approved product contract、request-local boundary 与 current mismatch 同时成立。

## S0 live 基线与 C12 关闭口径冻结

冻结：

- `pwd`、`HEAD`、latest commit、dirty boundary。
- C12-M1..M17 队列状态，至少确认 C12-M17 当前为空；必要时抽样或全量确认所有 C12 队列为空。
- 当前 capability 中非空 `remaining_gaps` / `known_gaps` 抽取结果为空。
- C12-M17 后 `SubtractivePipe product PipeLaw` 主 `Shape` lifecycle 已整改。
- PartDesign BSpline / 非 Line axis product extension 继续保留，不作为本包 bug。

S0 不改 C++、fixtures、expected、tests 或 capability source。

## S1 capability 与 publication authority 抽取

生成 `/tmp/c12m18-capabilities.json` 并结构化记录：

- 所有非空 `remaining_gaps`。
- 所有非空 `known_gaps`。
- 所有 `narrowed_gaps` path 和 key。
- `part_design.pipe`、`part_design.revolution_groove`、`part_workbench.sweep/filling/geomplate/loft/project_on_surface`、`assembly`、`sketcher`、`topo_history` 的当前 status 摘要。
- publication authority：`cad-core/src/runtime/capability_contract.cpp` 和相关 adapter assertions。

S1 输出应更新 source / scope / backend 矩阵，但不做 implementation 判断。

## S2 historical narrowed gap 三闸门复审

对每个 family 应用同一准入规则：

1. 是否存在 stable native expected、checked-in expected-backed fixture，或 approved product contract。
2. 是否能在 request-local DocumentObject graph / response update / diagnostics 边界中表达。
3. 是否有 current mismatch-confirmed 行。

必须逐类裁决：

- Groove UpTo product diagnostic。
- Sweep helper / location / advanced product-contract rows。
- Filling helper rows。
- GeomPlate native-hidden / oracle-blocked / product-contract rows。
- Loft subelement native-hidden row。
- ProjectOnSurface native mapper/provenance unavailable row。
- Assembly representative / marker / writeback rows。
- Sketch edge / stable id / split fragment ledger rows。
- SubShapeBinder CopyOnChange request-local support 与 native session cache边界。

S2 不允许把 `narrowed_gaps` 名称本身当作 implementation proof。

## S3 product extension 与 frontend sync 分流

复核当前剩余“可见差异”是否属于 backend C++：

- PartDesign 几何共线 BSpline / 非 Line 轴引用：保留为 CAD Core product extension；不改 strict FreeCAD parity；只要求 capability / expected / docs 不误称 native parity。
- C12-M11 / C12-M15 / C12-M16 的 sketch edge token、stable id、split fragment ledger：后端 current-supported；若产品仍有问题，默认分流到 `my-chili3d` frontend consumer sync。
- ProjectOnSurface / Sweep / Filling / GeomPlate / Loft 的 product-contract non-parity：若没有新 native expected 或 current mismatch，不创建后端实现包。

S3 输出应明确哪些项是 product extension retained、frontend package candidate、oracle package candidate 或 no-code retained。

## S4 next package authorization 裁决

S4 根据 S2/S3 输出三选一：

1. `implementation_package_authorized`：存在 mismatch-confirmed 行。必须写清最小完整语义批次、FreeCAD source authority、cad-core 落点、fixtures/tests、non-goals 和 validation。
2. `oracle_or_product_contract_package_required`：方向值得推进，但缺 stable expected 或 approved product contract。必须写清先采集/裁决什么，不能直接写 C++。
3. `frontend_sync_or_no_code_backlog_gate`：FreeCAD/cad-core 无当前实现项；若收益在前端，指向 my-chili3d 包；否则发布 no-code backlog gate。

S4 不允许因为“想继续 C12 编号”而授权实现包。

## S5 发布闸门

发布前确认：

- C12-M18 队列关闭后只输出表头。
- TSV 字段数、尾随空白和 `git diff --check` 通过。
- root README 记录 C12-M18 的最终出口和下一步分流。
- S4/S5 未授权 implementation package，也未授权 oracle/product-contract package。
- 后续只推荐外部 `my-chili3d` frontend sync package；FreeCAD/cad-core backend 发布 no-code gate 和重开条件，避免下一轮再次从 stale memory 重开。

## 非目标

- 不直接实现任何 C++。
- 不刷新 FreeCAD expected，除非 S4 明确授权后续 oracle package。
- 不把 `narrowed_gaps`、native-hidden、helper-blocked、oracle-blocked、product-contract non-parity 直接写成 supported。
- 不把 PartDesign BSpline / 非 Line axis product extension 改成 strict FreeCAD parity。
- 不处理 my-chili3d 前端代码；只允许把它作为后续分流。
- 不跑 full FreeCAD build 或全量 CI。

## 验收

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
