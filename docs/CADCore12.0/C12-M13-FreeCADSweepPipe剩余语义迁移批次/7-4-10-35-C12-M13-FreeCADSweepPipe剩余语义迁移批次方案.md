# C12-M13 FreeCAD Sweep / Pipe 剩余语义迁移批次方案

## 目标

在 C12-M12 已完成 multi-wire Pipe sewing 子路径后，继续迁移完整 FreeCAD Sweep / Pipe 中剩余的高价值语义，形成可执行、可验收、可拆分的 implementation 批次。

本轮采用“最小完整语义批次”：同一 FreeCAD 调用链下的代表场景一次性采 oracle、补实现、补 focused tests、回写 capability/docs。除非 FreeCADCmd 无法稳定采集、用户失败样例缺失或 helper API 不暴露必要状态，否则不再长期停留在单 fixture 推进。

## 范围切分

### S0 live 基线与 C12-M12 继承冻结

冻结当前 `HEAD`、dirty boundary、C12-M12 final status、现有 Pipe/Sweep fixture/test surface。S0 只允许写文档矩阵，不改代码。

S0 已关闭：live 基线为 `HEAD=592ee9f5b2`（`592ee9f5b2 feat: 支持 PartDesign Pipe 多线截面缝合`），dirty boundary 只有 root `docs/CADCore12.0/README.md` 与 C12-M13 docs/package；`cad-core/src`、tests、fixtures、other 均无 dirty。C12-M12 队列为空。继承口径为：multi-wire cap/sewing、fixed/round selected-spine current-supported、Part Sweep wrapper regression 可继承；vertex / wiresection diagnostics、AddSubShape/rawShape/Boolean lifecycle、Part Sweep mutable helper lifecycle 与 ORACLE-001 不能继承为完成。

### S1 source 与 current landing 批量复核

复核并记录：

- `FeaturePipe.cpp::Pipe::execute()` 中 `profilePoint`、`isLastSectionVertex`、`wiresections` 和 inner wire count 规则。
- `FeaturePipe.cpp::Pipe::execute()` 中 `AddSubShape.setValue(...)`、`rawShape`、no-base / fuse / cut / refine 后 `Shape` 生命周期。
- `FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` 对 downstream consumer 的 additive/subtractive cache 语义。
- `PartFeatures.cpp::Sweep::execute()` 与 `BRepOffsetAPI_MakePipeShellPyImp.cpp` 的 wrapper/helper mutable sequence。
- `cad-core` 当前落点：`feature_pipe.cpp`、`topo_shape_expansion.cpp`、`part_sweep.cpp`、`ComputeContext`、P7/P8 tests。

S1 已关闭：`C12M13-SRC-001..010` 均为 `reviewed`，open / waiting scope row 已绑定 source authority、current landing、S2 oracle owner 与 S3/S4/S5 implementation owner；`C12M13-BLOCKER-201` 已关闭。后续从 S2 oracle 批量采集继续，不等待 ORACLE-001 才推进 source-backed rows。

### S2 oracle 批量采集与用户复现分流

新增 `c12m13` fixture / expected，至少覆盖：

- profile 是单 vertex，section 是 wire 的 PartDesign Pipe。
- profile 是 wire，last section 是 vertex 的 PartDesign Pipe。
- inner wire 数不一致或中间 section vertex 的 FreeCAD diagnostic。
- AdditivePipe no-base / base fuse 的 `AddSubShape`、`rawShape`、final `Shape` 对照。
- SubtractivePipe base cut 的 removed tool cache 与 final Body 对照。
- Part Sweep helper mutable sequence：`add/remove/isReady/status/build/shape/simulate/makeSolid` 的 request-local response。
- ORACLE-001：若用户提供 failing request/result，则插入最小复现；若仍缺失，保持 `waiting_user_repro`，不得编造。

S2 关闭条件是每个实现子项都有 native/current 对照，或有明确 blocker；没有 current mismatch 的子项不进入 S3-S5 代码 gate。

S2 已关闭：

- `profile-point-to-wire-section` 与 `last-section-vertex` 已有 FreeCADCmd expected 和 current-focused green，不进入 S3 implementation。
- `invalid-middle-section-vertex-diagnostic` 的 S3 red evidence 已关闭：two-vertex diagnostic current-supported，unequal-inner-wire diagnostic 已对齐 FreeCADCmd catch-all 输出。
- AdditivePipe / SubtractivePipe lifecycle 已由 S4 red-to-green：FreeCAD feature `Shape` 为 post-boolean body，current feature 输出已对齐；Body final shape green 之外，focused test 还验证 replayed add/sub features 继续通过 AddSubShape tool cache 消费。
- Part Sweep helper 只采到 `add/isReady/getStatus/build/shape/makeSolid` subset；S5 已关闭为 `blocked_partial_helper_oracle`，`remove/firstShape/lastShape/generated/simulate` 仍需 dedicated native helper probe 或 approved product-contract artifact 才能重开。
- ORACLE-001 未收到用户 request/result，继续 `waiting_user_repro_non_blocking`。

### S3 multisection vertex 细节迁移

S3 已完成：`cad-core/src/part/topo_shape_expansion.cpp::preparePipeShellProfileLanes()` 仅在 shared PipeShell lane 准备层对齐 FreeCAD `Pipe::execute()` 的 unequal-inner-wire diagnostic boundary；后续 section wire 数多于 base lane 时返回 `A fatal error occurred when making the pipe`，不改 AddSubShape/rawShape 生命周期、不处理 Part Sweep helper、不引入 fixture-name 分支。

允许修改 `cad-core/src/part_design/feature_pipe.cpp` 与 `cad-core/src/part/topo_shape_expansion.cpp`，目标是对齐 FreeCAD：

- profile vertex 只能在 multisection 且至少有一段 section 时使用。
- last section vertex 只允许出现在最后一段。
- 每段 wiresection 的 inner wire 数必须按 FreeCAD 规则一致。
- vertex / wire mixed lane 的 `Add` / `SetLaw`、Simulate、cap/sewing、history 不靠后处理猜测。

### S4 Boolean / AddSubShape / rawShape 生命周期迁移

S4 已完成：`cad-core/src/part_design/feature_pipe.cpp` 现在把 Pipe producer 的 AddSubShape add/sub slot 保留为 pre-boolean tool cache；`cad-core/src/graph/recompute_plan.cpp` 为同一 Body 内的 Pipe 增加最近前序 PartDesign feature 依赖，使 Pipe executor 能在自身发布 feature `Shape` 时拿到 base 前缀并执行 Fuse/Cut。`partdesign-pipe-additive-lifecycle` 与 `partdesign-pipe-subtractive-lifecycle` expected 已移除 `known_gap`，`test_c12m13_partdesign_pipe_lifecycle_matches_native_oracle` 约束 feature Shape / Body final parity 以及 Body AddSubShape consumer replay。

允许修改 PartDesign Pipe executor 与 runtime cache，目标是锁定：

- `AddSubShape` 保存 pre-boolean tool cache。
- no-base additive 的 `rawShape` 是 refine 前 result，final `Shape` 是 refine 后 solid。
- additive base 走 Fuse，subtractive base 走 Cut，`result.Tag=-getID()` 的 pre-boolean tool owner 语义需要在 response/history 中可追踪。
- downstream `getAddSubShape()` consumer 可以从 request-local cache 读取 additive/subtractive tool，不靠 final shape 反推。

### S5 Part Workbench mutable helper 生命周期迁移

S5 已完成为 blocked/partial closure，未修改 `cad-core/src/part/part_sweep.cpp` 或 shared builder DTO。原因是 S2 只证明 `add/isReady/getStatus/build/shape/makeSolid` subset current-supported，未采证方法没有 checked-in native expected 或 approved product-contract artifact；临时 FreeCADCmd 调查还显示 helper 方法顺序敏感，组合 `remove/readd/simulate/build` 会触发 `NCollection_Sequence::ChangeValue`，不能作为稳定实现依据。

原 S5 目标保留为重开条件，必须先形成 dedicated native helper probe 或 product-contract artifact，再讨论 C++：

- constructor 必须接收 wire；invalid spine/wire 产生 FreeCAD 对齐 diagnostic。
- mode setters、tolerance、transition、profile `add/remove` 按顺序影响同一个 builder state。
- `isReady/status/build/shape/firstShape/lastShape/generated/simulate/makeSolid` 在 response 中区分未 build、build 失败、build success。
- Workbench `Part::Sweep` wrapper 仍只走 `Sweep::execute()` 主属性，不与 advanced helper product contract 混线。

### S6 集成回归与发布闸门

发布前必须：

- C12-M13 队列只剩表头。
- focused P7/P8 tests 通过。
- 新增 expected 与 current result 对齐。
- capability / adapter wording 如有状态变化已同步。
- root README、package README、方案、矩阵都写明 final status、剩余 blocker 和下一步。

## 实现顺序

1. 先采集 S2 oracle，不从 current fixture 输出倒推业务逻辑。
2. S3 只处理 multisection vertex / wiresection 规则。
3. S4 只处理 PartDesign AddSubShape / rawShape / Boolean 生命周期。
4. S5 只处理 Part Sweep helper lifecycle。
5. S6 做 focused regression 和发布，不临时补新功能。

## 最小验证命令

本轮短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次 docs/CADCore12.0/README.md
git diff --check
```

代码实现后 focused 验证候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c12m13_partdesign_pipe_lifecycle_matches_native_oracle tests.test_p7_features.CadCoreP7FeatureTest.test_c12m13_partdesign_pipe_vertex_success_paths_match_native_oracle tests.test_p7_features.CadCoreP7FeatureTest.test_c12m13_partdesign_pipe_vertex_wire_diagnostics_are_s3_red_evidence tests.test_p7_features.CadCoreP7FeatureTest.test_c12m12_partdesign_pipe_multiwire_sewing_matches_native_oracle
python3 -m unittest tests.test_p8_features
```

S2-S6 应把新增 `c12m13` focused tests 写入 validation matrix；普通步骤不要求全量 FreeCAD 构建或全量 CI。
