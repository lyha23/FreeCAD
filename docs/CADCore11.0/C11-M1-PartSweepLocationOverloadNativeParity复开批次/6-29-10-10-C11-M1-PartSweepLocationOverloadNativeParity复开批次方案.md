# C11-M1 PartSweep LocationOverload NativeParity 复开批次方案

## 背景

C10-M4 已把 SubShapeBinder CopyOnChange DTO 准入关闭为 docs-only retained diagnostic / release gate。当前 live capability 里没有新的直接 C++ `backend_gap`；`part_workbench.sweep` 反而有一条更明确的复开线：C6-M4 已实现 located profile 与 advanced combined CAD Core product contract，但仍保留 FreeCAD native wrapper build blocker evidence，状态是 non-parity。

本批次目标是判断 C6-M4 的 historical blocker 是否还能在当前 FreeCAD / LibPack / OCCT 基线下复现。如果 `add(Profile, Location, WithContact, WithCorrection)` 仍然不可采，则 C11-M1 只发布 retained non-parity；如果能够稳定采集，则进入 current cad-core comparison，并在 S6 打开具体代码 / expected / capability 升级。

## FreeCAD 调用链

- `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()`：native DocumentObject 路径只处理 `Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`，再调用 `TopoShape::makeElementPipeShell()`。
- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()`：wrapper 路径公开 `add(Profile, WithContact, WithCorrection)` 与 `add(Profile, Location, WithContact, WithCorrection)` 两种签名，Location overload 将 `Location` 转为 `TopoDS_Vertex` 并调用 OCCT `Add(s, v, ...)`。
- `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::setAuxiliarySpine()` 与 `setTolerance()`：combined case 的 wrapper 依据，只有在 Location overload 可采后才用于 parity comparison。

## cad-core 当前边界

- `cad-core/src/part/part_sweep.cpp` 已解析 `SectionOptions[].Location`、`ProfilePlacement`、`WithContact`、`WithCorrection`、AuxiliarySpine 和 Tolerance。
- `cad-core/src/part/topo_shape_expansion.cpp` 当前支持两条路径：native OCCT Location overload；以及 C6-M4 product contract 的 `AnchorLocationToSpineStart` 显式 placement。
- `cad-core/src/runtime/capability_contract.cpp` 发布 `part_workbench.sweep.status=supported_multi_profile_linearize_c6m4_product_contract_non_parity`，`remaining_gaps=[]`，并把 located profile / advanced combined FreeCADCmd wrapper blocker 保留在 `narrowed_gaps`。
- `cad-core/tests/test_p8_features.py`、`test_expected_fixtures.py`、`test_adapters.py` 已锁定 c5m10 known-gap guard 与 c6m4 product contract，不允许把 product contract 误报为 FreeCAD parity。

## 实施原则

- 先 native oracle，后 parity comparison；没有稳定 `shape_summary` 不打开 C++ gate。
- 先区分 native DocumentObject 与 Python wrapper：`Part::Sweep::execute()` 没有 advanced direct properties，不能把 wrapper product contract 宣称为 native DocumentObject parity。
- 只复开同一调用链：located profile、WithContact/WithCorrection、AuxiliarySpine、Tolerance、Transition、ProfilePlacement；不混入 Filling、Loft、GeomPlate、PartDesign Pipe/Hole。
- C++ 实现只在 S4 证明 current mismatch 时发生；禁止 fixture 名称分支、bbox/面积/输出排序、adapter 层业务逻辑和 cross-request TopoDS/BREP 状态。

## S0-S6 拆分

| 步骤 | 目标 | 关键输出 |
| --- | --- | --- |
| S0 | 冻结 live baseline 与声明口径 | README、总入口、capability grep、dirty boundary 和 validation matrix 对齐。 |
| S1 | 复核 FreeCAD 源码与 wrapper 候选 | source candidate TSV 回写 `Sweep::execute()`、`BRepOffsetAPI_MakePipeShellPy::add()`、current cad-core path。 |
| S2 | 做范围准入与 blocker 路由 | scope / blocker / non-goal / backend-gap TSV 全部有 owner step 和 close condition。 |
| S3 | FreeCADCmd 原生 Location overload 复采集 | 重新采 `add(Profile, Location, WithContact, WithCorrection)` 与 combined auxiliary/tolerance located section；写入 `notCollected` 或 stable oracle。 |
| S4 | ProductContract 到 Parity 升级审计 | 对比 native oracle 与 c6m4 current output；产生 `no_gap`、`diagnostic_retained` 或 `backend_gap_candidate`。 |
| S5 | 协议边界与 non-goal 复审 | 明确 GUI、native direct properties、persistent wrapper lifecycle、adapter 修剪和 fixture 特判不可进入本批次。 |
| S6 | Oracle 实现与发布闸门 | 有 backend gap 则落 C++ / fixtures / focused tests / capability；否则发布 no-code release gate。 |

## S6 代码落点规则

S6 只有在 S3-S4 产生 `backend_gap_candidate` 时才改代码。允许落点包括：

- `cad-core/src/part/part_sweep.cpp`：DTO 解析、diagnostics、metadata、ProfilePlacement / Location policy。
- `cad-core/include/cad_core/part/topo_shape_expansion.h` 与 `cad-core/src/part/topo_shape_expansion.cpp`：PipeShell section option、Location overload / product placement、NamedShape history。
- `cad-core/src/runtime/capability_contract.cpp`：从 non-parity / narrowed evidence 升级或保留状态。
- `cad-core/tests/test_p8_features.py`、`cad-core/tests/test_expected_fixtures.py`、`cad-core/tests/test_adapters.py`：focused parity / diagnostic / capability assertions。
- `cad-core/fixtures/c11m1` 或 current package documented fixture route：只有 native oracle stable 时才新增 expected。

禁止在 adapter 层补业务语义、按 fixture 名称分支、按 bbox/面积/输出顺序判定 PipeShell 结果、删除 c5m10 historical blocker evidence，或把 `PartDesign Pipe/Hole` 内部 PipeShell 当作 `Part::Sweep` support。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore11.0/C11-M1-PartSweepLocationOverloadNativeParity复开批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore11.0
git diff --check
```

代码闸门触发后：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
```

重型收口只在 S6 实际修改 collector、fixtures、capability 或核心 C++ 后执行；S0-S5 文档 / 准入步骤不跑 cad-core build。
