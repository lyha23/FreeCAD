# C12-M9 CAD Core implementation candidate 再盘点批次方案

## 目标

C12-M9 用 evidence-first 的方式回答“C12-M8 之后到底能实现什么”。它不默认选择 live capability 中唯一剩余的 CopyOnChange gap，因为 C12-M8 已证明该项缺 native copied graph evidence、缺 request-local DTO approval、也缺 current mismatch。

本包重新盘点：

- live capability 的 active `remaining_gaps`。
- C12-M1..M8 release gate 的关闭口径。
- `narrowed_gaps` 中的 historical native failure、native hidden、helper blocked、product-contract non-parity 与 current-covered 行。
- current tests / fixtures 是否已经证明某行 supported 或 product diagnostic contract。

## 批次边界

本包只做候选盘点、准入和后续包授权，不直接改 C++。

S5/S6 必须输出以下之一：

1. `implementation_package_authorized`：发现一条或一组最小完整语义批次，已具备 stable expected / product contract、request-local boundary 和 current mismatch；写清后续 implementation package 的范围。
2. `oracle_or_product_contract_package_required`：发现值得推进的方向，但缺 stable native expected 或缺产品契约，需要先另开 oracle / product-contract 包。
3. `no_code_backlog_gate`：没有行满足实现准入，继续保留 backlog 和重开条件。

## C++ 闸门

进入 implementation package 必须同时满足：

- FreeCAD source 或 product-contract source authority 可追溯到具体文件、类/函数和关键字段。
- 请求 / 响应只表达前端持久化 DocumentObject graph、`documentObjectUpdates`、diagnostics、mesh / subshape display output 或已批准的单次运行态产物。
- 不引入 backend session、temporary document cache、full BREP / TopoDS、persistent `NamedShape` / `ElementMap` cache。
- 有 focused tests 可以约束通用语义，不依赖 fixture 名、bbox、面积、输出排序或 adapter 修补。
- current mismatch 可用同一 request-local graph 或同一 product contract 复现。

## 步骤安排

- 入口：核对包结构和队列入口。
- S0：冻结当前 HEAD、dirty boundary、C12-M1..M8 队列、live capability snapshot 和 C12-M8 retained diagnostic 继承口径。
- S1：抽取 live `remaining_gaps`、known gaps、diagnostic codes、covered subset 和 capability publication source。
- S2：归类 `narrowed_gaps`：CopyOnChange、Groove UpTo、RuledSurface wire/wire、ProjectOnSurface、Sweep、Filling、GeomPlate、Loft、Assembly。
- S3：按 stable expected / product contract / current mismatch 三闸门筛出 candidate 或阻断原因。
- S4：对最高优先 candidate 做 source authority、cad-core 落点、fixtures / tests 和 non-goal 复核。
- S5：决定是否授权后续 implementation package；若授权，写明最小完整语义批次。
- S6：发布 C12-M9 最终状态，更新 CADCore12.0 README、矩阵和队列。

## 初始判断

当前最可能被误开的项是 CopyOnChange，但它刚在 C12-M8 被关闭为 `no_code_retained_diagnostic`。C12-M9 必须把它当作 retained blocker 输入，而不是默认实现方向。

当前更合理的盘点重点是 `narrowed_gaps`：确认哪些 row 已经是 current-covered / product-contract non-parity，哪些 row 仍缺 native expected，哪些 row 可能因为新的 expected 或 current regression 形成真实 mismatch。

## 验收分层

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M9-CADCoreImplementationCandidate再盘点批次 docs/CADCore12.0/README.md
git diff --check
```

候选复核：

```bash
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities > /tmp/c12m9-capabilities.json
jq -r 'paths as $p | select(getpath($p)|type=="array" and length>0 and ($p[-1]|tostring)=="remaining_gaps") | ($p|map(tostring)|join(".")) + " = " + (getpath($p)|@json)' /tmp/c12m9-capabilities.json
jq -r 'paths as $p | select(($p[-1]|tostring)=="narrowed_gaps") | ($p|map(tostring)|join("."))' /tmp/c12m9-capabilities.json
```

阶段回归只在 S5/S6 授权 implementation package 后执行；开包本身不跑 full build 或 full FreeCAD CI。

## 非目标

- 不直接实现 CopyOnChange full temporary document cache。
- 不把 `narrowed_gaps` 自动升级为 backend gap。
- 不把 native-hidden、helper crash、notCollected 或 product-contract non-parity 写成 current implementation mismatch。
- 不创建 C++ implementation package，除非 S5/S6 三闸门全部成立。
