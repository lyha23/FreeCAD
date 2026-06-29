# C12-M2 Part Workbench Native Oracle Probe 批次

## 当前定位

C12-M2 是 C12-M1 S6 `no_code_backlog_gate` 之后单独打开的 oracle collection / native probe 包。它不直接授权 C++、fixtures expected 改写、capability wording 或 adapter/test 改动；目标是把 C12-M1 S5 留下的 Part Workbench 历史证据重新采成稳定、request-local、可比较的 native expected。

本包只回答一个问题：Sweep / Filling / GeomPlate / Loft / ProjectOnSurface 这些 retained rows 里，是否能拿到同时满足 source authority、stable native expected、request-local/product boundary、current cad-core mismatch 的 implementation row。只有 S6 证明四项同时成立时，才允许另开后续 implementation 包。

## 入口

- 总入口：`6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次总入口.md`
- 方案：`6-29-18-53-C12-M2-PartWorkbenchNativeOracleProbe批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## S0 live 冻结

- `pwd=/Users/li/Chili3DProject/FreeCAD`
- `HEAD=4d245a9c11`
- `git log -1 --oneline=4d245a9c11 docs: 新增 C12-M2 native oracle probe 开包`
- `git -c core.quotepath=false status --short -uall=<clean>`
- C12-M1 队列检查只输出表头；C12-M2 队列在 S0 执行前从 S0-S6 开始。
- FreeCADCmd 只做发现性检查：`freecadcmd` 位于 `/Users/li/.cargo/bin/freecadcmd`。S0 未启动 FreeCAD，版本、OCCT/LibPack 与 sandbox/native runtime 分类由 S3 冻结。

S0 结论：C12-M2 是用户批准的 oracle/native probe 包，继承 C12-M1 S6 `no_code_backlog_gate`；代码 gate 仍关闭，本步只冻结 live baseline 和 oracle 声明口径，不采 expected。

## 范围

| family | C12-M1 retained reason | C12-M2 probe question | owner step |
| --- | --- | --- | --- |
| Sweep | no-code retained non-parity / notCollected | 是否能采到 Location overload / auxiliary spine / tolerance 的稳定 native expected，并证明 current cad-core mismatch。 | S4 |
| Filling | no-code retained non-parity / helper blocker | 是否能绕开 helper 生命周期噪声，采到 makeFilledFace / filling parameter 的稳定 native expected。 | S5 |
| GeomPlate | probe-only retained evidence | 是否能把 projected curve2d / G1 curve-on-surface 从 probe-only 证据升级成可比较 expected。 | S5 |
| Loft | native-hidden retained evidence | 是否能把 selected subelement assignment 的 native-hidden 行暴露成稳定 native expected。 | S4 |
| ProjectOnSurface | probe-only retained evidence | 是否能把 mapper / provenance 行采成 request-local expected，而不是只保留历史 probe。 | S5 |

## 出口分类

| classification | meaning | next action |
| --- | --- | --- |
| `oracle_expected_ready` | 已有 stable native expected、source authority、request-local 边界和 current mismatch。 | S6 可授权后续 implementation 包。 |
| `current_covered` | stable expected 存在，但 current cad-core 已一致。 | 不开代码，记录 covered。 |
| `native_probe_blocked` | FreeCAD native / helper / wrapper / hidden API 仍不能产出稳定 expected。 | 保留 blocker，必要时另开更窄 native probe 包。 |
| `product_boundary_rejected` | native 行依赖 GUI/session/persistent geometry，不适合 request-local CAD Core。 | 写入 non-goal registry。 |
| `retained_no_expected` | 仍只有 crash / timeout / notCollected / probe-only / native-hidden 历史证据。 | 不开代码。 |

## 禁止声明

- 不在本包修改 `cad-core/src`、`cad-core/include`、fixtures、expected、tests 或 adapters。
- 不把 crash、timeout、TypeError、notCollected、native-hidden、helper blocker 或 probe-only evidence 写成 implementation row。
- 不用当前机器系统 OCCT 差异替代正式 FreeCAD / LibPack oracle 基线。
- 不把 GUI / Workbench session、跨请求 BREP / TopoDS / NamedShape / ElementMap cache 引入 CAD Core request-local 边界。
- 不把单个 fixture 输出倒推成通用 FreeCAD 业务语义。

## 队列检查

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/工作步骤细分 --format markdown
```

## 文档验收

```bash
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M2-PartWorkbenchNativeOracleProbe批次 docs/CADCore12.0/README.md
git diff --check
```
