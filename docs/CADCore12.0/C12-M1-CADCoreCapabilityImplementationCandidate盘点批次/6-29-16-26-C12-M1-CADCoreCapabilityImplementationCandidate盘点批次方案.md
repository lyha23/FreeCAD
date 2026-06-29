# C12-M1 CAD Core capability implementation candidate 盘点批次方案

## 目标

C12-M1 用一个 evidence-first 闸门回答“下一轮到底实现什么”。它不从历史包里挑一个看起来还没做完的 fixture，而是从当前 `cad-core/cad-core capabilities` 和 CADCore9/10/11 release gate 反推是否存在可落 C++ 的 backend gap。

## 基线判断

- C11-M1 Sweep Location overload native parity 复开已关闭；当前没有 stable native expected，也没有 C++ gate。
- C11-M2 Filling native helper parity 复开已关闭；当前没有 stable native expected，也没有 C++ gate。
- CopyOnChange 仍是唯一 active `remaining_gaps`，但 C9-M5 / C10-M4 已裁定为 retained known gap / oracle blocked。
- Assembly representative solver、Part Workbench GeomPlate / Loft / ProjectOnSurface 等仍有 representative 或 narrowed evidence，但不是 active backend gap。

## 批次边界

本包做候选筛选和下一包授权，不做代码实现。S6 必须输出以下二选一结论：

1. 发现 implementation row：写清下一包主题、C++ 落点、FreeCAD authority、fixtures / focused tests、成功标准和禁止捷径。
2. 没有 implementation row：发布 no-code backlog gate，说明哪些行继续 retained、哪些行需要产品决策或 native oracle 才能重开。

S6 已选择第二种结果：发布 `no_code_backlog_gate`。C12-M1 没有授权下一轮 C++ implementation package；CopyOnChange、Assembly representative / marker / writeback、Part Workbench historical narrowed rows 均继续按各自 reopen condition 保留。

## C++ 闸门规则

任一候选只有同时满足以下条件，才允许进入下一轮代码包：

- FreeCAD source authority 明确，且语义能在 cad-core 无状态 request-local 边界内表达。
- 有 stable native expected、expected-backed fixture，或已批准的 product-contract expected。
- current cad-core 与 expected 存在可复现 mismatch。
- 不是 GUI、TaskPanel、persistent backend session、temporary document cache、cross-request BREP / TopoDS / NamedShape / ElementMap cache。
- focused tests 能锁定通用语义，而不是 fixture 名称、bbox、面积、输出排序或 adapter 层修补。

## 步骤安排

- S0 冻结 live 基线：C11 队列、capability JSON、唯一 active remaining gap、dirty boundary 和 forbidden claims。
- S1 复核 source candidates：`capability_contract.cpp`、`test_adapters.py`、FreeCAD source authority 和 CADCore9/10/11 release evidence。
- S2 完成 scope admission：给每行贴上 active gap、representative subset、historical narrowed、non-goal、release gate 或 implementable candidate。
- S3 专审 CopyOnChange：除非有更强 native oracle 与产品 DTO approval，否则保持 retained diagnostic。
- S4 专审代表子集：Assembly representative / marker / solver 边界只在产品明确扩大 request-local subset 时进入下一包。
- S5 专审历史 narrowed evidence：Sweep、Filling、GeomPlate、Loft、ProjectOnSurface 等只在 stable oracle 与 current mismatch 同时成立时重开。
- S6 发布闸门：已发布 `no_code_backlog_gate`，本轮无代码落点。

## 验收命令

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M1-CADCoreCapabilityImplementationCandidate盘点批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0
git diff --check
```

候选复核：

```bash
cd /home/user/Chili3DProject/FreeCAD
cad-core/cad-core capabilities > /tmp/c12-capabilities.json
python3 - <<'PY'
import json
data = json.load(open('/tmp/c12-capabilities.json'))
for area, vals in sorted(data.items()):
    if not isinstance(vals, dict):
        continue
    for name, item in sorted(vals.items()):
        if isinstance(item, dict) and (item.get('remaining_gaps') or item.get('narrowed_gaps') or item.get('non_goals')):
            print(area, name, item.get('status'), item.get('remaining_gaps'))
PY
```

阶段回归只在 S6 打开 C++ gate 后执行；S6 已关闭为 docs-only gate，不跑 cad-core build。

## 非目标

- 不实现 CopyOnChange full temporary-document cache。
- 不把 historical narrowed evidence 自动改成 active backend gap。
- 不把 representative fallback metadata 当作 solver product implementation。
- 不重开 C11-M1 / C11-M2 已关闭线，除非 S5 明确找到新的 stable oracle 和 current mismatch。
