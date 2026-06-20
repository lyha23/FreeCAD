# P8 Link / Assembly 运行时产品化主线总入口

本文是 `P8-LinkAssembly-运行时产品化主线` 的执行入口。后续实现应从本目录的 `工作步骤细分/` 队列启动，不直接续跑旧 P8 Assembly 子包中的残留入口。

## 主线目标

- 把完整 Link 账本、ShowElement 持久事务、cross-document hash / postfix 生命周期、多层 LinkSub / imported ElementMap、Assembly solver 扩展和 Worker / WASM / Web 合同合并为一个大功能模块。
- 让前端可持久化的 `DocumentObject graph` 成为唯一真实数据；CAD Core 只返回 shape、mesh、subshape、`elementReferenceUpdates`、`documentObjectUpdates`、diagnostics 和 capability。
- 所有实现必须先有 FreeCAD source authority，再落到 cad-core 的 `app` / `runtime` / `topo` / `assembly` / `adapters` 对应层。

## 执行队列

| 顺序 | 步骤 | 路径 | 目标 |
| --- | --- | --- | --- |
| 0 | 工作步骤总入口 | `工作步骤细分/6-20-18-05-【已实现】P8-LinkAssemblyRuntime工作步骤总入口.md` | 索引文件已建立，实际队列从 S0 开始 |
| 1 | S0 | `工作步骤细分/6-20-18-06-P8-LinkAssemblyRuntime-S0-live基线与旧队列裁决.md` | 复核 live baseline、旧队列和当前 capability |
| 2 | S1 | `工作步骤细分/6-20-18-07-P8-LinkAssemblyRuntime-S1-FreeCAD源码候选矩阵.md` | 精确 source authority 和 cad-core 落点 |
| 3 | S2 | `工作步骤细分/6-20-18-08-P8-LinkAssemblyRuntime-S2-Link账本与ShowElement事务.md` | 补完整 Link ledger 和 ShowElement 持久事务 |
| 4 | S3 | `工作步骤细分/6-20-18-09-P8-LinkAssemblyRuntime-S3-跨文档Hash与Postfix生命周期.md` | 补 cross-document hash / postfix / alias 生命周期 |
| 5 | S4 | `工作步骤细分/6-20-18-10-P8-LinkAssemblyRuntime-S4-多层LinkSub与ImportedElementMap.md` | 补多层 LinkSub 和 imported ElementMap |
| 6 | S5 | `工作步骤细分/6-20-18-11-P8-LinkAssemblyRuntime-S5-AssemblySolver扩展.md` | 在稳定 Link graph 上扩展 Assembly solver |
| 7 | S6 | `工作步骤细分/6-20-18-12-P8-LinkAssemblyRuntime-S6-WebRuntime合同冻结.md` | 冻结 Worker / WASM / Web runtime 合同并发布 |

## 旧包关系

- `P8-Assembly-Joint-Placement-OndselSolver收敛主线`、`P8-Assembly-Reference-JCS-MarkerPlacement收口主线`、`P8-DistanceType*`、`P8-*Joint-OndselSolver*` 是 source material 和 regression baseline，不作为本主线的当前队列入口。
- `docs/CADCore4.0/C4-M5-AssemblyRuntimeAdapter产品化主线` 已经完成 C4 维度收口；本包只消费其 Assembly / runtime / adapter 基线，不重复执行已关闭队列。
- S0 必须用 live code、fixtures、capabilities 和队列工具重新裁决 stale / covered / backendGap，不得只按旧 memory 或旧文档结论推进。

## 队列检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-LinkAssembly-运行时产品化主线/工作步骤细分 --format markdown
```

## 当前状态

本主线刚建立，S0-S6 均为待执行状态。后续每完成一步，应把对应步骤文件重命名为 `【已实现】`，同步更新矩阵和总入口，不允许只改代码不回写方案状态。
