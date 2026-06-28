# C10-M2 PartDesign DressUp / Hole TopoHistory 批次

本包承接 C10-M1 队列关闭后的下一轮 CAD Core 复核。主题是 PartDesign DressUp 与 Hole 的 request-local `NamedShape` / `ElementMap` / MapperHistory 第二阶段：先把 FreeCAD 源码依据、current capability、P7 expected 和 focused tests 对齐，再决定是否需要进入 cad-core C++ 实现。

当前 live capability 中 `part_design.hole.history.status=element_map_freeze_first_slice`，`topo_history.producer_matrix.dressup.status=done_first_slice`，`topo_history.producer_matrix.hole.status=done_first_slice`，且两者 `remaining=[]`。因此 C10-M2 默认不是“修一个已知 remaining gap”，而是用 C10-M1 同样的 S0-S6 闸门，查清 DressUp / Hole 生产者 history 是否存在新的 FreeCAD authority mismatch。

## 入口

- 批次总入口：`6-28-22-53-C10-M2-PartDesignDressUpHoleTopoHistory批次总入口.md`
- 批次方案：`6-28-22-53-C10-M2-PartDesignDressUpHoleTopoHistory批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前状态

- S0 已冻结 live baseline、状态词典、forbidden claims 和通用验证命令；S1-S6 仍为待执行。
- 工作步骤总入口已标 `【已实现】`，它只是队列索引，避免 goal runner 把索引当成实现步骤。
- `C10M2-BLOCKER-000=closed_s0`；`C10M2-SCOPE-001=baseline_frozen_s0` 只作为 S6 复核的 docs-only release baseline，不打开 C++ gate。
- 本包不声明新的 backend gap；`backend_gap_candidate` 必须等 S3-S5 给出 FreeCAD authority 或 checked-in expected 与 current cad-core 的 mismatch。
- C9/C10 保留的 `copy_on_change_full_temporary_document_cache` 仍是 SubShapeBinder retained known gap，不属于本包默认入口。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore10.0/C10-M2-PartDesignDressUpHoleTopoHistory批次 docs/CADCore10.0/README.md
git diff --check
```
