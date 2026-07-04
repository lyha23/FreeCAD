# C12-M13 FreeCAD Sweep / Pipe 剩余语义迁移批次

C12-M13 承接 C12-M12 的 `partial_implementation_multiwire_pipe_sewing` 出口，目标是继续实现完整 FreeCAD Sweep / Pipe 迁移中仍未关闭的后续项。

本包不重开 C12-M12 已证明 current-supported 的 fixed/round selected-spine、Part Sweep wrapper regression 或 multi-wire cap/sewing 子路径。它只处理 C12-M12 已写回为剩余项的四类范围：

- `PartDesign::Pipe::execute()` 的 multisection vertex 细节。
- `Pipe::execute()` 中 `rawShape`、`AddSubShape`、Boolean fuse/cut 与 Body Tip 的生命周期。
- `Part::Sweep` / `BRepOffsetAPI_MakePipeShellPy` mutable helper 生命周期。
- `C12M12-ORACLE-001` 用户失败复现的可插入 oracle 分支。

## 当前基线

- S0 live 基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- S0 live 基线：`HEAD=592ee9f5b2`（`592ee9f5b2 feat: 支持 PartDesign Pipe 多线截面缝合`）。
- S0 dirty boundary 按本次读取的 `git -c core.quotepath=false status --short -uall` 分组：docs=`M docs/CADCore12.0/README.md` 与未跟踪的 `docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/` 包；`cad-core/src`=`<none>`；tests=`<none>`；fixtures=`<none>`；other=`<none>`。非本包 dirty 未发现，本步不清理或回退任何用户改动。
- 队列基线：C12-M12 `工作步骤细分` 队列脚本只输出表头，确认已空；C12-M13 关闭 S0 前队列从 S0-S6 pending 开始，S0 完成后下一步应从 S1 `source 与 current landing 批量复核` 继续。
- C12-M12 可继承结论：final status 只能继承为 `partial_implementation_multiwire_pipe_sewing`；`cad-core/src/part/topo_shape_expansion.cpp` 的 multi-wire shell lane + shared cap/sewing 已有 `c12m12/partdesign-pipe-multiwire-sewing` fixture 和 P7 focused regression；fixed / round selected-spine 红灯已归因为 stale / wrong CMake cache 且刷新后 current-supported；Part Sweep wrapper / response regression 已由 P8 controls 证明 current-supported。
- C12-M12 不可继承为完成的结论：完整 FreeCAD Sweep / Pipe 迁移仍未关闭；profile vertex、last section vertex、inner wire count diagnostic、`AddSubShape` / `rawShape` / Boolean / Body Tip 生命周期、Part Sweep mutable helper lifecycle、ORACLE-001 用户失败复现仍归 C12-M13 后续步骤。
- 现有 focused surface：P7 PartDesign Pipe 覆盖 `c4m2` Additive/Subtractive Pipe、`c5m3` multisection / transition / diagnostics、`c51m4` selected-spine multisection / fixed-round / auxiliary-binormal、`c12m12` multi-wire sewing、`c6m3` interpolation law；P8 Part Sweep 覆盖 `c3m4` sweep shell/solid/transition/subedge/diagnostics、`c5m10` auxiliary/binormal/tolerance/advanced wrapper contracts、`c5m12` support surface normal、`c6m4` located-profile product/diagnostics controls。后续新增 `c12m13` 前必须复用这些作为回归面。
- S1 source / current landing 复核已关闭：`C12M13-SRC-001..010` 均为 `reviewed`，`C12M13-BLOCKER-201` 已关闭；open / waiting scope row 已写明 S2 oracle owner 与 S3/S4/S5 implementation owner。下一步从 S2 oracle 批量采集与用户复现分流继续，`ORACLE-001` 仍是非阻塞 `waiting_user_repro`。
- S2 oracle 批量采集已关闭：新增 `cad-core/fixtures/c12m13/` expected 与 focused tests。`ORACLE-101/102` 为 current-supported；`ORACLE-103` 的 unequal-inner-wire diagnostic 曾作为 S3 red evidence；`ORACLE-201/202` 证明 PartDesign Pipe feature `Shape` lifecycle 与 current pre-boolean/pre-cut tool 输出不一致，授权 S4；`ORACLE-301` 只覆盖 helper `add/isReady/getStatus/build/shape/makeSolid` 子集，`remove/firstShape/lastShape/generated/simulate` 阻塞 S5；`ORACLE-001` 仍是非阻塞 waiting row。
- S3 multisection vertex 细节迁移已关闭：`preparePipeShellProfileLanes()` 已对齐 FreeCAD `Pipe::execute()` 中后续 section 比 base 多 wire 时由 outer `catch (...)` 发布的 `A fatal error occurred when making the pipe`；`ORACLE-103` expected 移除 `known_gap`，`ORACLE-101/102` 和 `c5m3`、`c51m4`、`c12m12` focused regression 保持 green。下一步队列应从 S4 `Boolean / AddSubShape / rawShape 生命周期迁移` 继续。
- S4 Boolean / AddSubShape / rawShape 生命周期迁移已关闭：`feature_pipe.cpp` 现在把 AddSubShape 保留为 pre-boolean tool cache，同时对同一 Body 前序 PartDesign feature 建立 graph 依赖并在 Pipe producer 内发布 base Fuse/Cut 后的 feature `Shape` / mesh / subshapes / named shape；`ORACLE-201/202` expected 已移除 `known_gap`，Body replay 仍通过 AddSubShape add/sub slot 消费 tool。下一步队列应从 S5 `Part Sweep mutable helper 生命周期迁移` 继续。

## 问题定义

当前缺口不是“再找一个单 fixture 修补”，而是把 FreeCAD `FeaturePipe.cpp::Pipe::execute()` 和 Part Workbench mutable helper 的状态生命周期完整落到 `cad-core`。重点风险是：

1. multisection 中 profile point、last section vertex、wire count 对齐和 error message 只被粗略覆盖。
2. `AddSubShape.setValue(...)`、pre-boolean tool、post-boolean `boolOp` 与 refine 后 `Shape` 生命周期已由 S4 expected/test 锁住；剩余风险集中在 S5 helper mutable sequence。
3. AdditivePipe 与 SubtractivePipe 的 raw tool、fuse/cut owner、Body Tip 和 downstream AddSubShape consumer 已由 S4 focused test 约束；DressUp / Transformed 旧 consumer surface 不在 S4 重开。
4. Part Workbench helper 的 `add/remove/isReady/status/build/shape/firstShape/lastShape/generated/simulate/makeSolid` 这类 mutable sequence 还没有作为 request-local 生命周期模型验证。
5. ORACLE-001 仍缺用户 request/result；它可以随时插入 S2/S3，但不能阻塞无关的 source-backed 剩余项。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M13 用法 |
| --- | --- | --- |
| multisection vertex / wire 分组 | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | `profilePoint`、`isLastSectionVertex`、`wiresections`、inner wire count 和 vertex-only section error 的源权威。 |
| cap/sewing 后 AddSubShape | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | `AddSubShape.setValue(result.makeElementCompound(...))` 必须锁定为 pre-boolean tool cache，不等同最终 `Shape`。 |
| rawShape / Boolean / refine | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | no-base additive、base fuse、base cut、pre-refine `rawShape` 与 post-refine `Shape` 的生命周期。 |
| AddSubShape consumer | `src/Mod/PartDesign/App/FeatureAddSub.cpp::FeatureAddSub::getAddSubShape()` | downstream DressUp / Transformed 读取 additive/subtractive tool cache，而不是倒推 final shape。 |
| Part Sweep wrapper | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | Workbench `Part::Sweep` 只发布 `Sections/Spine/Solid/Frenet/Transition/Linearize` 主 wrapper。 |
| PipeShell mutable helper | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` | helper 状态序列、mutable mutation、diagnostics 和 request-local DTO 的源权威。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/src/part_design/feature_pipe.cpp` | PartDesign Pipe executor、Body replay、AddSubShape cache、rawShape / Shape response。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | shared PipeShell builder、multisection vertex / wire lane、cap/sewing / history。 |
| `cad-core/src/part/part_sweep.cpp` | Part Sweep wrapper parser、advanced helper DTO、mutable helper lifecycle diagnostics。 |
| `cad-core/src/runtime/compute_context.*` | `AddSubShape` / raw named shape / Body Tip 生命周期需要的 request-local cache。 |
| `cad-core/tests/test_p7_features.py` | PartDesign Pipe focused oracle 和 AddSubShape / rawShape lifecycle tests。 |
| `cad-core/tests/test_p8_features.py` | Part Sweep helper lifecycle focused tests。 |
| `cad-core/fixtures/c12m13/` | 本包新增 FreeCADCmd expected 和用户失败最小复现。 |

S1 复核后的当前 landing 口径：PartDesign Pipe 的 current path 已落在 `feature_pipe.cpp` 与 `topo_shape_expansion.cpp`，但 AddSubShape / rawShape / final Shape 是否需要拆成 FreeCAD 等价通道必须由 S2/S4 oracle 决定；Part Sweep wrapper 已由 `part_sweep.cpp::executePartSweep` 覆盖，mutable helper lifecycle 只准在 S2/S5 证据成立后单独处理。

## 入口

- 总入口：`7-4-10-35-C12-M13-FreeCADSweepPipe剩余语义迁移批次总入口.md`
- 方案：`7-4-10-35-C12-M13-FreeCADSweepPipe剩余语义迁移批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 预期出口

1. `implementation_complete_for_remainder_batch`：multisection vertex、AddSubShape/rawShape/Boolean lifecycle、Part Sweep helper lifecycle 均有 source-backed oracle、focused tests 和 current green。
2. `partial_implementation_with_named_followups`：某一子项被 FreeCADCmd / OCCT / user repro 阻塞，其余子项完成并留下精确 reopen condition。
3. `blocked_by_oracle_or_helper_visibility`：native helper 生命周期或用户失败样例无法稳定采集，不补 C++，只保留 blocker。

## S2 oracle 结论

- `cad-core/fixtures/c12m13/partdesign-pipe-profile-point-to-wire-section.json` 与 `partdesign-pipe-last-section-vertex.json` 已有 FreeCADCmd expected，focused P7 证明 current-supported，不授权 S3 修改这两条成功路径。
- `partdesign-pipe-vertex-wire-diagnostics.json` 的 diagnostic-only S3 gap 已关闭：两 vertex section 诊断与 FreeCAD 一致；unequal inner wire 已对齐 FreeCADCmd catch-all fatal，expected 不再保留 `known_gap`。
- `partdesign-pipe-additive-lifecycle.json` 与 `partdesign-pipe-subtractive-lifecycle.json` 已由 S4 关闭：native feature `Shape` 是 post-boolean body，current feature 输出已 red-to-green；Body final geometry 继续 green，且 replayed add/sub feature 列表证明仍通过 AddSubShape tool cache 消费。
- `part-sweep-helper-mutable-sequence.json` 已证明 helper collected subset current-supported；未覆盖 `remove`、`firstShape`、`lastShape`、`generated`、`simulate`，S5 不应实现这些未采证方法。

## 非目标

- 不实现完整 Topological Naming。
- 不重写所有 Part surface family。
- 不把 mesh response 当成 BRep parity 证据。
- 不把 C12-M12 已关闭的 fixed/round selected-spine 或 multi-wire sewing 重做一遍。
- 不等待 ORACLE-001 才推进 source-backed 的剩余项；ORACLE-001 只在用户提供 request/result 后插入验证流。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M13-FreeCADSweepPipe剩余语义迁移批次 docs/CADCore12.0/README.md
git diff --check
```
