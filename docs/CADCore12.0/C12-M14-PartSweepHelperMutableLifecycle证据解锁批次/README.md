# C12-M14 Part Sweep helper mutable lifecycle 证据解锁批次

C12-M14 承接 C12-M13 的 `partial_implementation_with_named_followups` 出口，专门处理 `C12M13-ORACLE-301` 留下的 Part Workbench `BRepOffsetAPI_MakePipeShellPy` mutable helper 未采证方法。

本包不重开 C12-M13 已关闭的 PartDesign Pipe 语义：unequal-inner-wire diagnostic、AddSubShape pre-boolean tool cache、base Fuse / Cut 后 feature `Shape` 生命周期、Body replay consumer、multi-wire cap/sewing 和 Part Sweep wrapper no-mix regression 均作为已关闭回归面继承。

本包只处理：

- `BRepOffsetAPI_MakePipeShellPy.remove(...)`
- `firstShape()`
- `lastShape()`
- `generated(...)`
- `simulate(...)`
- 上述方法与 `add/isReady/getStatus/build/shape/makeSolid` 的最小 request-local 调用顺序、失败/成功状态和 response 字段。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=8ef7a10b6a`（`8ef7a10b6a 文档：关闭 C12-M13 S6 发布闸门`）。
- 创建前 `git -c core.quotepath=false status --short -uall` 无输出；本轮只新增 `docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/` 并更新 `docs/CADCore12.0/README.md`。
- S0 live 冻结：`HEAD=09e2f66c73`（`09e2f66c73 文档：新增 C12-M14 helper 生命周期证据方案`），`pwd=/Users/li/Chili3DProject/FreeCAD`，起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S0 dirty boundary：docs=`<none>`；`cad-core/src`=`<none>`；tests=`<none>`；fixtures=`<none>`；other=`<none>`。非本包 dirty 未发现，本包后续不覆盖或回退非本包改动。
- S0 队列冻结：C12-M13 `工作步骤细分` 队列为空；C12-M14 队列从 S0 开始，S0 关闭后下一步进入 S1 source 与 current helper landing 复核。
- S1 source/current landing 复核：`HEAD=055237df6c`（`055237df6c 文档：关闭 C12-M14 S0 基线冻结`），起点 worktree clean。已复核 FreeCAD helper binding、plain `Sweep::execute()` no-mix 边界、`part_sweep.cpp` current response 字段、`topo_shape_expansion.cpp` 内部 `Simulate(2)` 用途和 C12-M13 focused subset。
- S2 dedicated native helper probe schema 与采集：`HEAD=0251a16d10`（`0251a16d10 文档：关闭 C12-M14 S1 source landing 复核`），起点 worktree clean。已新增 `docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-schema.md`、`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe.py`、`docs/temp/7-4-12-15-c12m14-helper-lifecycle-native-probe-output.json` 与 `docs/temp/7-4-12-15-c12m14-helper-lifecycle-freecadcmd-version.txt`。FreeCAD baseline 为 `1.2.0 revision 20260519` / OCCT `7.8.1`；`remove/firstShape/lastShape/generated/simulate` 均有 stable payload 或 stable diagnostic，组合 `remove/readd/simulate/build` 记录 `NCollection_Sequence::ChangeValue` 为 `native_instability_blocker`。
- S3 product contract 与 current mismatch 准入裁决：`HEAD=fa3fde076d`（`fa3fde076d 文档：关闭 C12-M14 S2 helper native probe`），起点 worktree clean。S2 artifact + source/current audit 确认 `ORACLE-101..104` 是 `implementation_authorized`：native stable expected/diagnostic 成立，当前 `part_sweep.cpp` 缺 helper lifecycle response 字段。`ORACLE-105` 是 `product_contract_only`：native `NCollection_Sequence::ChangeValue` 不稳定不能称为 FreeCAD parity，只能按本包 `7-4-13-26-C12-M14-helper-lifecycle-request-local产品契约.md` 作为 CAD Core request-local product contract 推进；S4 后续只执行这些授权/契约行，不重开 PartDesign Pipe 或 plain `Part::Sweep` wrapper。
- S4 helper lifecycle 实现收口：`HEAD=c80329d633`（`c80329d633 文档：关闭 C12-M14 S3 helper 契约裁决`），起点 worktree clean。已在 `cad-core/src/part/part_sweep.cpp` 新增 opt-in `HelperLifecycle` request DTO 与 per-operation response，覆盖 remove、firstShape/lastShape、generated、standalone simulate 和 request-local remove/readd/simulate/build 产品契约；`ORACLE-105` 明确输出 `native_parity=false` 与 `contract_provenance=cad_core_product_contract_non_parity`，不称为 FreeCAD native parity。plain `Part::Sweep` wrapper 输出保持不变，`topo_shape_expansion.cpp` cap/sewing 内部 `Simulate(2)` 未混入 helper simulate。
- S5 发布闸门：`HEAD=6be8764a2d`（`6be8764a2d 实现 C12-M14 S4 helper 生命周期契约`），起点 worktree clean。最终发布 `implementation_unlocked_helper_lifecycle` + `product_contract_published_helper_lifecycle` 混合状态：`ORACLE-101..104` 是 source-backed helper lifecycle current-supported rows，`ORACLE-105` 只按 CAD Core request-local product contract 关闭，保留 `native_parity=false` 与 `contract_provenance=cad_core_product_contract_non_parity`。Capability / adapter 公开口径已补 `HelperLifecycle` DTO、C12-M14 fixture、source-backed fields 与 ORACLE-105 non-parity contract；队列关闭后应只输出表头。
- C12-M13 已发布 `partial_implementation_with_named_followups`：S3/S4 已完成，S5 保持 `blocked_partial_helper_oracle`。
- C12-M13 `ORACLE-301` 已证明 helper collected subset `add/isReady/getStatus/build/shape/makeSolid` current-supported；`remove/firstShape/lastShape/generated/simulate` 缺 checked-in native expected 或 approved product-contract artifact。
- C12-M13 S5 的临时 FreeCADCmd 调查只能证明单独调用可观察，组合 `remove/readd/simulate/build` 会触发 `NCollection_Sequence::ChangeValue`，不能作为稳定 expected。

## 问题定义

当前缺口不是普通 `Part::Sweep` wrapper 或 PartDesign Pipe executor，而是 FreeCAD Python helper 的 mutable builder 生命周期。它的风险点是：

1. helper 是有状态对象，`add/remove` 的调用顺序会改变内部 section 列表和 builder readiness。
2. `firstShape/lastShape/generated/simulate` 依赖 build state 和 OCCT helper 内部历史，不能只从 final mesh 或 wrapper output 倒推。
3. FreeCAD native probe 若不稳定，不能把临时 crash/异常当成 CAD Core 支持状态；必须记录为 native helper artifact blocker 或转入明确产品契约裁决。
4. CAD Core 的 `part_sweep.cpp` 当前主要表达 wrapper 与 advanced DTO，不应在没有证据时冒充完整 mutable Python object parity。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M14 用法 |
| --- | --- | --- |
| Workbench Sweep wrapper | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | wrapper 主路径 no-mix guard；不得把 helper lifecycle 强塞回普通 Sweep wrapper。 |
| Python helper binding | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` | `add/remove/isReady/getStatus/build/shape/firstShape/lastShape/generated/simulate/makeSolid` 的源权威。 |
| shared PipeShell builder | `src/Mod/Part/App/TopoShapeExpansion.cpp::makeElementPipeShell()` | 仅在 native/product-contract 证据成立后映射到 cad-core request-local builder。 |
| CAD Core wrapper landing | `cad-core/src/part/part_sweep.cpp` | 当前 Part Sweep wrapper / advanced DTO / helper metadata 落点。 |
| CAD Core PipeShell builder | `cad-core/src/part/topo_shape_expansion.cpp` | shared builder options、simulate/cap/sewing 不得误写成 helper API parity。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/src/part/part_sweep.cpp` | Part Sweep wrapper parser、advanced helper DTO、未来 helper lifecycle response。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | request-local PipeShell builder 和 OCCT `BRepOffsetAPI_MakePipeShell` 调用点。 |
| `cad-core/tests/test_p8_features.py` | Part Sweep wrapper/helper focused tests。 |
| `cad-core/fixtures/c12m14/` | S4 新增 `part-sweep-helper-mutable-lifecycle.json` 与 expected，覆盖 source-backed helper lifecycle 和 ORACLE-105 request-local product contract。 |
| `docs/temp/` | 临时 FreeCADCmd helper probe 脚本与输出；只有稳定 schema 输出才可升级为 expected。 |

## 入口

- 总入口：`7-4-11-54-C12-M14-PartSweepHelperMutableLifecycle证据解锁批次总入口.md`
- 方案：`7-4-11-54-C12-M14-PartSweepHelperMutableLifecycle证据解锁批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`
- S3 request-local product contract：`7-4-13-26-C12-M14-helper-lifecycle-request-local产品契约.md`

## 预期出口

1. `implementation_unlocked_helper_lifecycle`：dedicated native probe 或 approved product contract 覆盖未采证方法，current mismatch 成立，后续 S4 可以实现。
2. `product_contract_published_helper_lifecycle`：native helper parity 不稳定，但 CAD Core request-local lifecycle DTO 被明确批准并有 focused tests。
3. `no_code_retained_helper_blocker`：native probe / product contract 仍不成立，继续保留 blocker，不改 C++。

## 最终出口

- 已发布混合状态：`implementation_unlocked_helper_lifecycle` + `product_contract_published_helper_lifecycle`。
- `ORACLE-101..104`：source-backed helper lifecycle rows，已由 S4 fixture/expected/focused P8 test 关闭为 current-supported。
- `ORACLE-105`：仅为 CAD Core request-local product contract，不能称为 FreeCAD native parity；只有同一 FreeCAD / LibPack / OCCT baseline 产出 stable native expected 且不再触发 `NCollection_Sequence::ChangeValue` 时才重开 native parity。
- S5 后 blocker queue 无 dangling open row；`C12M14-BLOCKER-302` 保留明确 reopen condition，`C12M14-BLOCKER-601` 已关闭。

## 非目标

- 不重开 PartDesign Pipe S3/S4。
- 不重写 `Part::Sweep` wrapper 主属性路径。
- 不用 mesh response 证明 helper method parity。
- 不把临时 FreeCADCmd crash 当成 supported。
- 不等待 `ORACLE-001` 用户失败复现；它属于独立 PartDesign Pipe follow-up。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/temp docs/CADCore12.0/README.md
git diff --check
```
