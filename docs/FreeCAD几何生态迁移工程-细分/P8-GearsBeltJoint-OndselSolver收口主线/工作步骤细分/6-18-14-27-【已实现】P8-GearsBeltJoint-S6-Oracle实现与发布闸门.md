# P8 GearsBeltJoint S6 Oracle 实现与发布闸门

当前状态：已实现。S6 只做最终 gate 复核和文档发布收口；本轮未修改 C++、fixture 或 expected。

## 目标

消费 S2-S5 留下的 `unsupportedImplementable`、`notCollected` 和 `releaseGate`，把 Gears / Belt 作为最小 request-local JointType 支持发布，并明确 remaining unsupported / nonGoal 边界。

## 输入

- `GBJ-BLOCK-001` 到 `GBJ-BLOCK-006`
- FreeCAD source authority：`AssemblyObject::makeMbdJointOfType()`、`AssemblyObject::makeMbdJoint()`、`JointObject.py::JointUsingDistance2`
- c3m6 fixture / expected
- hard-linked real Ondsel build

## 关闭结论

- `GBJ-BLOCK-001` 已关闭：`JointConstraint.distance2`、request parser 和 `solver_joints[].distance2` 输出存在且未回退。
- `GBJ-BLOCK-002` 已关闭：Gears / Belt 映射到 `ASMTGearJoint`，`radiusI=Distance`；Gears `radiusJ=Distance2`，Belt `radiusJ=-Distance2`，`isSupportedOndselJointType()` 只新增这两个 JointType。
- `GBJ-BLOCK-003` 已关闭：Gears c3m6 fixture 与 FreeCADCmd expected 入库并通过 `--check`，focused runtime test 断言 `unsupported_joints=[]`、`radius_i=2.0`、`radius_j=1.0`。
- `GBJ-BLOCK-004` 已关闭：Belt c3m6 fixture 与 FreeCADCmd expected 入库并通过 `--check`，focused runtime test 断言 `unsupported_joints=[]`、`radius_i=2.0`、`radius_j=-1.0`。
- `GBJ-BLOCK-005` 已关闭：C ABI capabilities 与 `test_adapters.py` 发布 `Gears` / `Belt` supported，`unsupported_joint_matrix` 只保留 `RackPinion` / `Screw`。
- `GBJ-BLOCK-006` 已关闭：P8 AssemblySolver 与本包 TSV 保持 RackPinion / Screw diagnostic-only，复杂 Distance 仍为 notCollected，GUI/session 仍为 nonGoal。

## scope 结论

| scope | S6 结论 | 证据 |
| --- | --- | --- |
| `GBJ-SCOPE-002` | supported | S3 DTO / adapter 映射、S4 Gears native oracle、S5 capability publication 和 S6 focused validation 均通过 |
| `GBJ-SCOPE-003` | supported | S3 Belt 负 `radius_j` 映射、S4 native oracle、S5 capability publication 和 S6 focused validation 均通过 |
| `GBJ-SCOPE-004` | supported | Gears / Belt FreeCADCmd expected 已入库，两个 `--check` 均以退出码 0 通过 |
| `GBJ-SCOPE-005` | supported | C ABI capabilities、focused tests、GBJ/P8ASM docs 与 TSV 已同步 |
| `GBJ-SCOPE-006` | unsupported | RackPinion / Screw 仍在 unsupported matrix；需要独立 marker / sliding-part 方案 |
| `GBJ-SCOPE-007` | notCollected | 复杂 Distance geometry 未进入本包，仍等待 DistanceType-specific oracle |
| `GBJ-SCOPE-008` | nonGoal | GUI drag、postDrag、persistent solver session 继续排除在 stateless CAD Core 边界外 |

## 最终验证记录

| 命令 | 结果 |
| --- | --- |
| `cmake --build build --target cad-core cad_core_ffi` | 通过；`cad-core` 与 `cad_core_ffi` 均构建完成 |
| `python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_unsupported_joint_stays_diagnostic` | 通过；`Ran 3 tests`，`OK` |
| `FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check` | 通过；FreeCAD `1.2.0devR20260519`，退出码 0，无 expected mismatch |
| `FREECADCMD=/home/user/.local/bin/FreeCADCmd python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check` | 通过；FreeCAD `1.2.0devR20260519`，退出码 0，无 expected mismatch |
| GBJ 与 P8ASM 两个矩阵目录 TSV field-count check | 通过；无列数不一致输出 |
| `git diff --check -- cad-core docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线 docs/FreeCAD几何生态迁移工程-细分/P8-Assembly-Joint-Placement-OndselSolver收敛主线` | 通过 |
| `python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/工作步骤细分 --format markdown` | 通过；队列为空 |

## 发布边界

S6 发布的是最小 request-local Gears / Belt JointType 支持：输入来自请求里的 DocumentObject graph，输出是单次 recompute 的 solver metadata 和 placement updates。它不是完整 Assembly transaction lifecycle，不保存跨请求 solver state，也不声明 GUI drag / postDrag 行为。

## 剩余风险

- 本包内没有剩余 blocker；remaining unsupported 明确收敛为 RackPinion / Screw。
- 复杂 Distance geometry 没有 native oracle，不作为 Gears / Belt 标量 `Distance` / `Distance2` 支持的一部分。
- S4 曾记录 broad expected suite 中非本包 `p7/hole-supported-model-thread-counterbore` volume delta 略高于容差；本轮未运行 broad expected suite，也未修改该 fixture 或 expected。

## 实施顺序核查

1. 复核 S3 已关闭的 `GBJ-BLOCK-001`：`JointConstraint.distance2` 与 Gears / Belt 的 `Distance` / `Distance2` request builder 不回退。
2. 复核 S3 已关闭的 `GBJ-BLOCK-002`：`ASMTGearJoint` include、Gears / Belt factory mapping 和 supported predicate 不回退。
3. 复核 S4 已关闭的 `GBJ-BLOCK-003`：Gears c3m6 fixture、native expected、focused runtime assertion 不回退。
4. 复核 S4 已关闭的 `GBJ-BLOCK-004`：Belt c3m6 fixture、native expected、focused runtime assertion 与负 `radius_j` 不回退。
5. 复核 S5 已关闭的 `GBJ-BLOCK-005`：`c_api.cpp` capabilities 和 `test_adapters.py` 未回退。
6. 复核 S5 已关闭的 `GBJ-BLOCK-006`：既有 P8 docs / TSV 只移除 Gears / Belt 的 unsupported 状态，保留 RackPinion / Screw。

## 代码落点

| blocker | C++ / 测试 / 文档落点 | 成功标准 |
| --- | --- | --- |
| `GBJ-BLOCK-001` | `cad-core/include/cad_core/assembly/joint_solver.h`、`cad-core/src/assembly/joint_solver.cpp` | 已由 S3 关闭；S6 只复核 `JointConstraint.distance2` 与 request builder 不回退 |
| `GBJ-BLOCK-002` | `cad-core/src/assembly/joint_solver.cpp` | 已由 S3 关闭；S6 只复核 `Gears` / `Belt` 的 `ASMTGearJoint` 映射与 supported predicate 不回退 |
| `GBJ-BLOCK-003` | `cad-core/fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json`、expected、`cad-core/tests/test_p8_features.py` | 已由 S4 关闭；S6 只复核 real_ondsel_solver focused test 和 expected parity 不回退 |
| `GBJ-BLOCK-004` | `cad-core/fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json`、expected、`cad-core/tests/test_p8_features.py` | 已由 S4 关闭；S6 只复核 expected parity 与负 `radius_j` 不回退 |
| `GBJ-BLOCK-005` | `cad-core/src/adapters/c_api/c_api.cpp`、`cad-core/tests/test_adapters.py` | supported matrix 加入 Gears / Belt，unsupported matrix 移除它们 |
| `GBJ-BLOCK-006` | 既有 P8 AssemblySolver docs / matrices、本包 TSV | RackPinion / Screw 仍为 unsupported；complex Distance 仍为 notCollected；GUI/session 仍为 nonGoal |

## 禁止路径

- 禁止恢复 representative fallback 或 unlinked build support。
- 禁止靠 fixture 名称、bbox、volume、输出顺序或 shape 数量推断 JointType。
- 禁止在 adapter 层补业务语义。
- 禁止把 RackPinion / Screw 顺手映射到 supported。
- 禁止把完整 Distance geometry、GUI drag、postDrag 或 persistent session 混入本包。

## 验收标准

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check -- ../docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线 src/assembly/joint_solver.cpp include/cad_core/assembly/joint_solver.h src/adapters/c_api/c_api.cpp tests/test_p8_features.py tests/test_adapters.py fixtures/c3m6
```

native oracle gate：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-gears-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-belt-joint-real-solver.json --check
```

阶段回归：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

发布前 TSV 检查：

```bash
for f in /home/user/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P8-GearsBeltJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

关闭条件：

- `GBJ-SCOPE-002/003/004/005` 全部从 unsupported / notCollected / releaseGate 转为 `supported`，且证据记录在本文件或后续 `【已实现】` S6 文件中。
- `GBJ-SCOPE-006` 保持 `unsupported`，`GBJ-SCOPE-007` 保持 `notCollected`，`GBJ-SCOPE-008` 保持 `nonGoal`。
- `git status --short` 中只包含本轮相关文件，且无 build 产物 / `__pycache__` 混入。

## 非目标

- 不运行全量 FreeCAD 构建。
- 不实现 RackPinion / Screw。
- 不调整 unrelated P5/P6/P7 docs 或 matrices。
