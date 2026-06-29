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

## S1 source 基线

S1 已把五个 family 的 source authority 回填到 source/probe 矩阵：每行都有 FreeCAD 源文件、类/函数和关键短句，不存在缺 source authority 仍进入后续 probe 的行。证据分三类保留：

- `existing expected`：C5/C6 已有 checked-in fixture expected 或 product-contract/non-parity fixture，只能作为历史上下文或 current comparison target。
- `historical probe output`：C11-M1 Sweep 与 C11-M2 Filling 的 native probe JSON 只证明 `notCollected` / helper lifecycle / diagnostic control，不是 stable native expected。
- `no-code retained`：C12-M1 S5/S6、C11 release gate 和 C6 release gate 的 retained / product-contract 结论保持 no-code，不升级成 implementation candidate。

S1 未运行 FreeCADCmd/native probe，未修改 `cad-core/src`、fixtures、expected、tests、adapters 或 capability wording。`C12M2-BLOCKER-003` 关闭为 `closed_s1_none_found`；稳定 native expected 与 current mismatch 仍由 S4/S5/S6 判定。

## S2 范围准入

S2 已逐行复核 C12M2-SRC/SCOPE/CAT/PROBE/BLOCKER：Sweep 为 `probe_admitted`，由 S4 关闭 `C12M2-BLOCKER-101`；Filling 为 `helper_blocked`，由 S5 分离 helper lifecycle 与稳定 expected；GeomPlate 与 ProjectOnSurface 为 `needs_probe_design`，分别由 S5 处理 wrapper / mapper 证据；Loft 为 `native_hidden_blocked`，由 S4 判断是否能暴露可比较 expected。

全局 blocker 保留为：FreeCADCmd runtime baseline（S3）、probe artifact schema（S3）和 current cad-core comparison path（S6）。`C12M2-BLOCKER-003` 保持 `closed_s1_none_found`。Non-goal registry 已覆盖 GUI/session、persistent geometry / cross-request native cache、full BREP product API、crash/timeout/notCollected/helper lifecycle 噪声，以及 API/output-order provenance guessing。Backend gap classification 仍保持 `oracle_probe_candidate` 或 `retained_no_expected`，代码 gate 关闭。

## S3 harness / FreeCADCmd 基线

S3 已固定 C12-M2 native probe artifact schema 为 `c12m2.native-probe-artifact.v1`，并提供 file-backed harness，避免 FreeCADCmd 长 `-c` 字符串不稳定。S4/S5 probe artifact 必须记录 probe id、family、case id、source authority、input artifact、FreeCADCmd path/version、OCCT/LibPack、命令、stdout/stderr、exit code、异常分类、expected summary、request-local 判定、current comparison path 和结论。

S3 baseline artifact：

- schema：`docs/temp/6-29-20-12-c12m2-native-probe-schema.md`
- harness：`docs/temp/6-29-20-12-c12m2-native-probe-harness.py`
- baseline probe：`docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-probe.py`
- baseline output：`docs/temp/6-29-20-12-c12m2-freecadcmd-baseline-native-probe.json`

当前 FreeCADCmd baseline 为 `/Users/li/.cargo/bin/freecadcmd`，FreeCAD `1.2.0 revision 20260519`，OCCT `7.8.1`，LibPack / LibPackVersion 为空。该 baseline 的 `expected_ready` 只表示 runtime metadata 可读，不表示任何 family geometry expected 已发布；S4/S5 仍需按 family 产出 `expected_ready`、`native_probe_blocked`、`helper_blocked`、`native_hidden`、`sandbox_runtime_limit`、`collector_bug`、`product_boundary_rejected` 或 `retained_no_expected`。

## S4 Sweep / Loft probe 结论

S4 已按 S3 schema 运行 file-backed harness，FreeCADCmd 为 `/Users/li/.cargo/bin/freecadcmd`，FreeCAD `1.2.0 revision 20260519`，OCCT `7.8.1`，LibPack / LibPackVersion 为空。新增 artifacts：

- `docs/temp/6-29-21-55-c12m2-s4-sweep-loft-native-probe.py`
- `docs/temp/6-29-21-55-c12m2-s4-sweep-native-probe-output.json`
- `docs/temp/6-29-21-55-c12m2-s4-sweep-options-native-probe-output.json`
- `docs/temp/6-29-21-55-c12m2-s4-loft-subelement-native-probe-output.json`

Sweep final classification：Location overload 为 `native_probe_blocked`。fresh probe 中 located representatives 均在 `is_ready_before_build=true` / `status_before_build=0` 后于 build 阶段返回 `OCCError: NCollection_Array1::Value`；plain no-location control 可 build。auxiliary / binormal / tolerance / no-location combined controls 可作为 current-covered c5m10 context，但带 Location 的 combined row 仍依赖 Location blocker。`C12M2-BLOCKER-101` 已关闭，S6 不得对 Location overload 做 current mismatch 比较。

Loft final classification：selected subelement 为 `native_hidden`。object-level `Sections` control 可 build；`[(object, ["Edge1"]), ...]` 等 tuple subelement assignment 被 `App::PropertyLinkList` 拒绝，错误为 `TypeError: Type must be App.DocumentObject or None, not tuple`。`C12M2-BLOCKER-401` 已关闭，c5m12/c6m7 继续只是 native-hidden / product-contract context。

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
