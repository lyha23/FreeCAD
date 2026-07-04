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
| `cad-core/fixtures/c12m14/` | 若 S2/S3 准入成立，本包新增 native helper probe / product-contract fixture。 |
| `docs/temp/` | 临时 FreeCADCmd helper probe 脚本与输出；只有稳定 schema 输出才可升级为 expected。 |

## 入口

- 总入口：`7-4-11-54-C12-M14-PartSweepHelperMutableLifecycle证据解锁批次总入口.md`
- 方案：`7-4-11-54-C12-M14-PartSweepHelperMutableLifecycle证据解锁批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 预期出口

1. `implementation_unlocked_helper_lifecycle`：dedicated native probe 或 approved product contract 覆盖未采证方法，current mismatch 成立，后续 S4 可以实现。
2. `product_contract_published_helper_lifecycle`：native helper parity 不稳定，但 CAD Core request-local lifecycle DTO 被明确批准并有 focused tests。
3. `no_code_retained_helper_blocker`：native probe / product contract 仍不成立，继续保留 blocker，不改 C++。

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
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/CADCore12.0/README.md
git diff --check
```
