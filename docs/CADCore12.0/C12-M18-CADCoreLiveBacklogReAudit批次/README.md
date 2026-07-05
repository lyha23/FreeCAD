# C12-M18 CAD Core live backlog re-audit 批次

C12-M18 是 C12-M17 关闭后的 live backlog 复审包。它不预设下一个 C++ implementation 目标，而是重新从当前 `cad-core` capability、C12-M1..M17 release gate、`narrowed_gaps`、非原生产品扩展边界和当前测试面中筛选是否存在新的可实现项。

当前 live 结论是：C12-M17 队列已关闭，当前 capability 中没有非空 `remaining_gaps` 或 `known_gaps`。仍存在的 `narrowed_gaps` 是历史 native-hidden、helper-blocked、oracle-blocked、product-contract non-parity 或 current-covered 记录，不能直接升级为实现包。C12-M18 的任务是把这个事实结构化冻结，并定义下一轮可开包的准入闸门。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=14bbd0ceb9`（`14bbd0ceb9 feat: 修复 SubtractivePipe product PipeLaw 主 Shape 生命周期`）。
- 创建时 worktree clean。
- C12-M17 队列只输出 markdown 表头。
- `cad-core/build/cad-core capabilities` 当前没有任何非空 `remaining_gaps`。
- `cad-core/build/cad-core capabilities` 当前没有任何非空 `known_gaps`。
- live `narrowed_gaps` 仍存在于：
  - `part_design.revolution_groove`
  - `part_workbench.filling`
  - `part_workbench.geomplate`
  - `part_workbench.loft`
  - `part_workbench.project_on_surface`
  - `part_workbench.sweep`
- `docs/capability/7-5-00-14-cad-web-background非FreeCAD原生语义边界.md` 当前只保留 PartDesign 几何共线 BSpline / 非 Line 轴引用产品扩展为当前非原生差异；C12-M17 已整改 SubtractivePipe product PipeLaw 主 `Shape` lifecycle。

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
- S0：冻结 live HEAD、dirty boundary、C12-M1..M17 队列闭合状态和 C12-M17 后的 capability 空 gap 事实。
- S1：抽取 current `remaining_gaps`、`known_gaps`、`narrowed_gaps`、非原生产品扩展和 publication authority。
- S2：对历史 `narrowed_gaps` family 做 stable expected / product contract、request-local boundary、current mismatch 三闸门复审。
- S3：裁决 product extension 与 frontend consumer sync：保留 axis extension，分离 my-chili3d consumer work，不把前端缺口误写成后端 C++ gap。
- S4：授权下一包或 no-code gate：若存在 mismatch-confirmed 行，写清最小完整语义批次；否则输出 oracle/product-contract/frontend/no-code 分流。
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
