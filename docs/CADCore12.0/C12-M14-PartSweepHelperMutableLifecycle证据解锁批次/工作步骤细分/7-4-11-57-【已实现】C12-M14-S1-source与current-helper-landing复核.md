# 【已实现】C12-M14 S1 source 与 current helper landing 复核

## 目标

把 C12-M14 的 helper method 集合绑定到 FreeCAD source、cad-core 当前落点和 focused P8 surface。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`055237df6c`。
- `git log -1 --oneline`：`055237df6c 文档：关闭 C12-M14 S0 基线冻结`。
- `git -c core.quotepath=false status --short -uall`：无输出。
- S1 开始前队列：S1-S5 pending，第一项为本步骤。

## FreeCAD source authority

### `BRepOffsetAPI_MakePipeShellPyImp.cpp`

`src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` 是本包 helper mutable lifecycle 的 source authority。

- `PyMake()` 通过 spine wire 创建 `new BRepOffsetAPI_MakePipeShell(...)`，因此 helper 是 request 内有状态 builder。
- `add(Profile, WithContact, WithCorrection)` 调 `Add(s, ...)`；`add(Profile, Location, WithContact, WithCorrection)` 调 `Add(s, v, ...)`。
- `remove(Profile)` 调 `Delete(s)`，返回 `Py_Return`。
- `isReady()` 调 `IsReady()`，返回 Python boolean。
- `getStatus()` 调 `GetStatus()`，返回 Python long。
- `makeSolid()` 调 `MakeSolid()`，返回 Python boolean。
- `build()` 调 `Build()`，返回 `Py_Return`。
- `shape()` 调 `Shape()`，返回 `TopoShapePy`。
- `firstShape()` / `lastShape()` 调 `FirstShape()` / `LastShape()`，返回 `TopoShapePy`。
- `generated(shape)` 调 `Generated(s)`，返回 `Py::List` of `TopoShapePy`。
- `simulate(nbsec)` 调 `Simulate(nbsec, list)`，返回 `Py::List` of `TopoShapePy`。
- 上述方法都用 `catch (Standard_Failure& e)` 写 `PartExceptionOCCError` 并返回 `nullptr`；wrapper 本身没有用 `IsReady()` 或 `Build()` 保护 `shape/firstShape/lastShape/generated/simulate`，所以 S2 必须采集未 build、build fail、build success 三态，而不能从 final mesh 倒推。

### `PartFeatures.cpp::Sweep::execute()`

`src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` 是 plain `Part::Sweep` wrapper source authority，不是 Python helper mutable API。

- wrapper 只读取 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`。
- 主路径先解析 spine 和 section `TopoShape`，再调用 `result.makeElementPipeShell(shapes, isSolid, isFrenet, transMode, Part::OpCodes::Sweep)`。
- 成功后仅 `this->Shape.setValue(result)`；源码没有 `remove/firstShape/lastShape/generated/simulate` 调用点。
- 因此 wrapper no-mix guard 成立：C12-M14 helper lifecycle 不能混入普通 `Sweep::execute()` parity；plain wrapper 只能作为已关闭回归面。

## cad-core current landing

### `cad-core/src/part/part_sweep.cpp`

- `executePartSweep()` 当前对齐 `Sweep::execute()` wrapper，接受 `Sections/Spine/Solid/Frenet/Linearize/Transition` 以及已存在 advanced DTO 字段。
- `readAdvancedSweepOptions()` 只解析辅助 spine、support mode、binormal、section options、tolerance 等 request-local DTO，并把 `advanced` metadata 写入 response。
- 当前 response 主要包含 `feature/spine/sections/solid/frenet/transition/linearize/topo_naming_history/advanced`；location overload 只发布 known-gap/product-contract metadata。
- 文件中没有 `remove/firstShape/lastShape/generated/simulate` 的 helper lifecycle response 字段；S3 前不能把 current output 写成 helper parity 证据。

### `cad-core/src/part/topo_shape_expansion.cpp`

- `makeElementPipeShellFromSources()` 是 shared PipeShell builder landing：创建局部 `BRepOffsetAPI_MakePipeShell`，设置 mode/transition/tolerance，按 section `Add()`，再走 `IsReady()`、`Build()`、可选 `MakeSolid()` 和 maker history。
- 文件内 `Simulate(2)` 只在 `sewCaps` / open shell cap-sewing 流程中使用，用于提取 front/back wire 并配合 `BRepBuilderAPI_Sewing`。
- 该内部 `Simulate(2)` 对齐 FreeCAD PartDesign Pipe cap/sewing，不是 Python helper `simulate(nbsec)` API parity；S2 仍需 dedicated helper probe 或产品契约。

### focused regression surface

- `cad-core/tests/test_p8_features.py::test_c12m13_part_sweep_helper_mutable_sequence_supported_subset_matches_native_oracle` 只保护 C12-M13 collected subset。
- `cad-core/fixtures/c12m13/part-sweep-helper-mutable-sequence.json` 是 `Part::Sweep` input，expected 的 `oracle_boundary.covered_methods` 只有 `add/isReady/getStatus/build/shape/makeSolid`。
- 同一 expected 明确 `uncollected_methods=["remove","firstShape","lastShape","generated","simulate"]`，下一步仍必须由 S2/S3 决定。

## 矩阵回写

- `c12m14_helper_lifecycle_source_matrix.tsv`：`C12M14-SRC-001..006` 均关闭为 `reviewed_s1`，并记录 source 短句 / current landing 边界。
- `c12m14_helper_lifecycle_scope_matrix.tsv`：每条 scope row 均补齐 source authority、current landing、S2 oracle owner、S3 gate owner 和 S4 owner。
- `c12m14_helper_lifecycle_blocker_queue.tsv`：`C12M14-BLOCKER-201` 关闭。
- `c12m14_helper_lifecycle_validation_matrix.tsv`：`C12M14-VAL-101` 记录实际 `rg` 命令和结论。
- `c12m14_helper_lifecycle_non_goal_registry.tsv`：新增 wrapper no-mix guard，确认 plain `Sweep::execute()` 不作为 helper lifecycle parity 证据。

## 结论

- S1 只完成 source/current landing 复核；未采集 oracle，未运行 FreeCADCmd。
- 未修改 C++、fixtures、expected 或 tests。
- S2 可继续设计 dedicated native helper probe schema；S3/S4 仍需等待 probe/product-contract/current-mismatch gate。

## 非目标

- 不采集 oracle。
- 不修改 C++。
- 不新增 fixture。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'BRepOffsetAPI_MakePipeShellPy|firstShape|lastShape|generated|simulate|remove|Sweep::execute' src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp src/Mod/Part/App/PartFeatures.cpp
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/CADCore12.0/README.md
git diff --check
```
