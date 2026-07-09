# C13-M4 FreeCADExpectedLedger TopoState 投影闭环批次总入口

## 目标

把 `docs/框架/7-9-15-53-FreeCADCmd权威账本与topoNamingState裁剪原则.md` 落到 CADCore13.0 的实现批次中：先保证 checked-in expected 与 FreeCADCmd ledger sidecar 闭合，再让 `cad-core` runtime 输出对齐 public `.freecad.json` 投影。

## 入口文件

- README：`README.md`
- 方案：`7-9-18-40-【已实现】C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 当前结论

- `.freecad.json` 是对外协议 expected；同名 `.freecad.ledger.json` 是 FreeCADCmd 权威账本 sidecar。
- `c4m6` 的 ledger validator 已作为本批次最小完整语义批次门禁，9/9 green。
- runtime 已发布 `Compound.Child0.Face1` 这类 child path 投影，focused topoNamingState runtime 14 tests OK。
- C13-M4 只关闭 `c4m6` public projection 闭环；没有新增 C13-M2/C13-M3 回流 blocker。

## 使用方式

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/工作步骤细分 --format markdown
```

## 第一轮执行顺序

1. 跑 S0，记录 live baseline 与首个 diff，不改代码。
2. 跑 S1，只修 child path projection 发布。
3. 跑 S2，让 `c4m6` focused parity 转绿。
4. 跑 S3，收口 docs/矩阵/索引，并确认没有新增 C13-M2/C13-M3 回流 blocker。

## 普通验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore13.0/C13-M4-FreeCADExpectedLedgerTopoState投影闭环批次/矩阵/*.tsv
git diff --check
```
