# C12-M6 RuledSurface Wire/Wire 准入验证批次总入口

## 目标

验证当前 `Part::RuledSurface` wire/wire 支持是否满足正式准入条件，并裁决旧 `PARTSURF-BLOCK-005` 是应关闭为 current-supported、保留 blocker，还是另开 implementation candidate。

## 当前假设

当前 live code / capability 已显示 wire/wire 可能已经实现：`part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 包含 `c4m1/part-ruled-surface-wire-wire`。C12-M6 必须核实该口径，不直接把旧 C3M4 `deferred-S2` 当成仍未实现。

## S0 live 冻结

- S0 执行基线：`pwd=/Users/li/Chili3DProject/FreeCAD`，`HEAD=71dba06fb6`（`71dba06fb6 docs: 完成 C12-M5 S5 发布闸门`）。
- 起点 dirty boundary 只包含 `docs/CADCore12.0/README.md` 修改和未跟踪的 C12-M6 包。
- C12-M1..M5 队列均为空；C12-M6 S0 开始前队列为索引、S0、S1、S2、S3、S4、S5。
- `cad-core/build/cad-core capabilities` 显示 `part_workbench.ruled_surface.status=supported_wire_wire_expected_backed`，fixtures 含 `c4m1/part-ruled-surface-wire-wire`，`remaining_gaps=[]`，covered / request-local boundaries 均包含 `wire_wire_brepfill_shell`。
- 旧 deferred 继承口径冻结为 collector、input schema、shell/topo provenance 三项；下一步为 S1 source/current evidence 复核。

## 执行规则

1. 每步开始前执行 live baseline：`pwd`、`git rev-parse --short HEAD`、`git log -1 --oneline`、`git -c core.quotepath=false status --short -uall`。
2. 每步只关闭本步骤 owner 的矩阵行和 blocker；S2/S3/S4 三闸门全部通过前，不发布 `wire_wire_admitted_current_supported`。
3. 若发现 unrelated dirty work，不回退、不覆盖；只读取或暂停确认。
4. 不从 fixture 输出倒推业务逻辑；必须用 FreeCAD source、checked-in expected、current recompute 和 focused tests 共同裁决。
5. 本包默认只改 docs / matrices；只有 S5 明确分类为 `implementation_candidate_required` 后，才另开代码实现包。

## 顺序

- S0：live 基线与继承口径冻结。
- S1：FreeCAD 源码与旧 PARTSURF 证据复核。
- S2：collector / expected 准入验证。
- S3：request-local input schema 准入验证。
- S4：shell/topo provenance 准入验证。
- S5：implementation gate 与发布闸门。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M6-RuledSurfaceWireWire准入验证批次/矩阵/*.tsv
git diff --check
```
