# C7-M5 Transformed Pattern MapperHistory 复杂 ownership 实现准入主线总入口

## 结论

C7-M5 是 C7-M4 之后的 P7 follow-up。总览后续队列明确下一项是 P7 transformed / pattern 完整 MapperHistory 与更复杂 ownership；P8 扩展排在其后。

当前默认 gate closed：旧 P7 Transformed 主线已经关闭基础 topology_counts 和 supported / covered 发布闸门，C7-M5 不能凭“复杂 ownership”这个名称直接改 C++。S0-S2 必须先复核 live source、fixtures、expected 和 tests，形成可采 native oracle 的最小完整语义批次。S4 只有在 source-backed native oracle 证明 current `cad-core` mismatch 时，才允许把 S5 转成 implementation。

工作步骤总入口索引已按 C7-M1/C7-M2/C7-M3 约定标记为 `【已实现】`，只用于跳过索引文件；S0 已冻结 live 基线与旧 P7T 已关闭边界，S1 已完成 FreeCAD source 与 current `cad-core` coverage 复核，S2 已裁出 S3 native oracle 候选批次。当前下一 pending 必须是 S3。

## 上游状态

- C7-M4 release gate 已完成，最终提交为 `1f2b86990d 文档：完成 C7-M4 S5 发布闸门`。
- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md` 的后续队列：P5/P6 已收口，P6 MakerHistory 当前无 C++ backendGap，下一项是 P7 transformed / pattern 完整 MapperHistory 与复杂 ownership。
- 旧 P7 Transformed 主线：`docs/FreeCAD几何生态迁移工程-细分/P7-Transformed-Topology-MakerHistory收敛主线/` 已关闭 S0-S6，基础 transformed topology oracle 和 ownership / fallback evidence 为 supported / covered。
- P7 live 口径：`docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md` 声明 transformed family 超出当前 fixture 矩阵的复杂参数组合按 unsupported diagnostic 或后续专项处理。
- C7-M5 S0 已完成：live 起点 `HEAD=a2cc93a1ee`（`a2cc93a1ee 文档：收口 C7-M5 工作步骤总入口索引`），开始状态干净；C7-M1/C7-M2/C7-M3/C7-M4 和旧 P7 Transformed 队列均为空。旧 P7T `P7T-SCOPE-001..007` 保持 `supported`，`P7T-BG-001/002` 保持 supported/covered closed，`P7T-BG-003` 保持 standalone lifecycle boundary，`P7T-BLOCK-001..005` 均 closed，`P7T-NG-005` 保持 standalone Whole shape nonGoal，冻结表写入 `矩阵/c7m5_transformed_history_p7_boundary_freeze.tsv`；S0 未采 oracle、未改 C++、fixtures、expected 或 tests。
- C7-M5 S1 已完成：`HEAD=27b2f84d6a` 起步且工作区干净；已复核 FreeCAD `Transformed::execute()`、`DressUp::getAddSubShape()`、`MultiTransform::getTransformations()`、`TopoShape::makeElementTransform()`，以及 current `cad-core` transformed copy alias、terminal/merge history、P7 fixture/test 覆盖；S2 输入池写入 `source_authority.tsv`、`scope.tsv` 和 `blocker_queue.tsv`。S1 未采 oracle、未改 C++、fixtures、expected 或 tests。
- C7-M5 S2 已完成：`HEAD=cbfbfe736d` 起步且工作区干净；`C7M5-SCOPE-101` 关闭为 `already_covered`，`C7M5-SCOPE-201` 和 `C7M5-SCOPE-301` 进入 `oracle_candidate`，`C7M5-SCOPE-401` 保持 `diagnostic_non_goal`，未打开 `backend_gap_candidate`。S3 只处理 support-backed fixture 清单，不采 standalone Whole shape native golden。

## 初始范围

- Mirrored / LinearPattern / PolarPattern / Scaled / MultiTransform 的复杂 ownership 和 MapperHistory。
- Features 模式下 AddSubShape slot ownership、链式 DressUp / SupportTransform、multi-original add / sub replay。
- Whole shape support-backed owner、Body prefix support、refined prefix support 和 terminal / merge / split / deleted history。
- MultiTransform child template composition、TransformN alias、original stable alias、ElementMap copy 和 source retag。

## 排除项

- C7-M4 stale `ReferenceShadow` / Base recovery。
- standalone geometry-equivalent native golden，尤其缺 Body / BaseFeature lifecycle 的 Whole shape 用例。
- GUI、TaskPanel、完整 DressUp universe、full MapperHistory 全量迁移。
- P8 Assembly / Link / Worker / WASM / Web service bridge。

## 步骤队列

工作步骤总入口索引不是实现步骤；其文件为 `工作步骤细分/6-26-08-37-【已实现】C7-M5工作步骤总入口.md`。当前执行队列：

1. S0：已完成，冻结 live baseline、C7-M1..M4 队列和旧 P7 Transformed 已关闭边界。
2. S1：已完成，复核 FreeCAD source、当前 `cad-core` topo/history 能力和 fixture/test 覆盖。
3. S2：已完成，形成复杂 ownership native oracle 候选矩阵和最小完整语义批次。
4. S3：下一 pending，采集 native oracle 或记录 oracle blocker / diagnostic non-goal。
5. S4：用 current `cad-core` 做 parity 和 implementation gate 裁决。
6. S5：实现正式 ownership / MapperHistory gap，或 no-code 发布收口。
7. S6：release gate，更新 README / 矩阵 / P7 口径并清空队列。

## 验收入口

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M5-TransformedPatternMapperHistory复杂ownership实现准入主线 docs/CADCore7.0/README.md
git diff --check
```
