# C13-M5 FreeCADExpected 发布对齐批次总入口

## 目标

把 `cad-core` 的正式发布输出对齐到 `cad-core/fixtures/<phase>/expected/*.freecad.json`。本批次先建立严格比较和分层推进机制，再按 phase 家族修 runtime、topo、feature 或 diagnostics 缺口。

## 入口文件

- README：`README.md`
- 方案：`7-10-00-15-C13-M5-FreeCADExpected发布对齐批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前边界

- `.freecad.json` 是 native expected，作为 public expected 目标。
- `.freecad.ledger.json` 是 sidecar 证据，不进入 runtime。
- `*.expeted.json` 是协议手写合同，不纳入本批次自动 expected discovery。
- `cad-core-res/*.cad-core.json` 是生成物，只能由当前 cad-core recompute 重生成。
- 随机 raw hash 只在 comparator 层 canonicalize，不改变 expected 或 runtime。

## 使用方式

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/工作步骤细分 --format markdown
```

## 入口关闭条件

- 包结构齐备：README、方案、总入口、S0-S5、矩阵。
- TSV 字段数校验通过。
- 顶层 `CADCore13.0/README.md` 已加入 C13-M5 索引。
- 本入口关闭后队列从 S0 开始。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次/矩阵/*.tsv
git diff --check -- docs/CADCore13.0/C13-M5-FreeCADExpected发布对齐批次 docs/CADCore13.0/README.md
```
