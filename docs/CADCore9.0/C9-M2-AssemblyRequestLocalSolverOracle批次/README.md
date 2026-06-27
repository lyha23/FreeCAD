# C9-M2 Assembly request-local solver oracle 批次

## 定位

C9-M2 承接 C9-M1 no-code closure 后的剩余 Assembly request-local solver evidence。它不把后续工作压缩成单个 bundled `offsetPlc` fixture，而是把同一 FreeCAD 调用链里的 oracle、focused tests、capability 和可能的 C++ 落点一次拆清。

## 当前状态

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d52cd67a19`（`d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`）。S0 起始 status 仅包含 `docs/CADCore9.0/README.md` 与本 C9-M2 seed 文档 / 矩阵 / step 文件，未出现 cad-core source、fixture、expected 或 tests 改动。
- C9-M1 队列为空，C9-M1 已关闭且不重开；C9-M2 只处理 Assembly request-local solver oracle 批次。
- live capability 仍发布 `assembly.remaining_gaps=[]`、`subshape_marker_placement.remaining_gaps=[]`，`assembly.ondsel_solver_adapter.status=covered_full`，`placement_writeback.status=covered_full`。
- `non_identity_bundled_offsetPlc` 已由 S6 关闭为 request-local supported：marker 解析按 FreeCAD `handleOneSideOfJoint()` 应用 bundled `offsetPlc`，writeback 按 `setNewPlacements()` / `validateNewPlacements()` 把 solved MbD placement 转回 document placement。
- `assembly-marker-custom-placement-chain-real-solver` 已有 expected；C9-M1 记录它未被 focused tests 直接断言，S4 已把 exact fixture 接入 focused test 并锁定 identity offset boundary。
- `non_assembly_link_subshape_primitive_frame_generalization` 仍是 diagnostic non-goal；zero Angle exact-zero fallback 已由 S6 关闭为 expected-backed current parity。

## 批次边界

| 方向 | 当前状态 | C9-M2 目标 |
| --- | --- | --- |
| bundled `offsetPlc` object marker | expected_backed_current_match | S6 已按 `data.offsetPlc * markerPlacement` 关闭，object marker `[0.25,0.5,0.75] -> [2.25,0.5,0.75]`。 |
| bundled `offsetPlc` subshape marker | expected_backed_current_match | S6 已按同一 request-local part offset 关闭，subshape marker `[0.25,0.5,0.75] -> [2.25,0.5,0.75]`。 |
| bundled `offsetPlc` writeback | expected_backed_current_match | S6 已按 `getMbdPlacement(mbdPart) * offsetPlc` 关闭，`ComponentC` writeback 为 `[6,0,2]`。 |
| custom placement-chain expected | focused test activated | S4 已直接断言 `assembly-marker-custom-placement-chain-real-solver` expected，且锁定 identity offset boundary；不宣称 bundled offset parity。 |
| zero Angle fallback | expected_backed_current_match | S6 exact-zero Angle fallback 关闭，`ComponentB` writeback 为 `[4,0,4]`，旧 `ondsel_solver_failed` route 不再作为该 fixture 的 supported route。 |
| primitive frame generalization | diagnostic_non_goal | 保持 non-goal，除非产品另批 DTO |

S0 关闭证据：C9-M1 queue script 只输出表头；C9-M2 queue 在 S0 重命名前从 S0 开始；`cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 仍断言 Assembly remaining gaps 为空、`non_identity_bundled_offsetPlc` 和 primitive frame generalization 在 non-goals 内。本步未采 native oracle、未改 cad-core 源码 / fixtures / expected / tests，也未运行 build 或重型回归。

## S1 关闭结论

- S1 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=8dc1ec2ccd`（`8dc1ec2ccd docs: 关闭 C9-M2 S0 基线冻结`），S1 起始 `git -c core.quotepath=false status --short -uall` 无输出。
- FreeCAD source authority 已固化到 `source_candidates`：`AssemblyObject::getMbDData()` 在 `bundleFixed` 下把 fixed-joint connected part 复用同一 `ASMTPart`，并把 `objectPartMap[partToAdd].offsetPlc` 写成 `plc.inverse() * plci`；`handleOneSideOfJoint()` 的 marker 顺序是 object-global、part-local，再 `data.offsetPlc * plc`；`validateNewPlacements()` / `setNewPlacements()` 的 solver writeback 顺序是 `getMbdPlacement(mbdPart) * offsetPlc`。
- `makeMbdJointOfType()` 的 FreeCAD authority 明确为 Angle 0 或 2pi fallback 到 `ASMTParallelAxesJoint`；cad-core 当前落点在 `joint_solver.cpp::makeOndselJointOfType()`，S5 仍需 native expected 判定 current parity，不在 S1 宣称 supported。
- cad-core 落点已分类：marker / Angle 在 `joint_solver.cpp`，writeback JSON 在 `assembly_utils.cpp`，request-local display apply 在 `assembly_object.cpp`，capability 和 adapter tests 仍保持 `non_identity_bundled_offsetPlc` 与 primitive frame generalization 为 non-goals 且 Assembly remaining gaps 为空。
- S1 当时确认 `assembly-marker-custom-placement-chain-real-solver.freecad.json` 已存在，但 exact fixture name 只命中 expected 文件，focused tests 尚未直接断言；该项交给 S4 激活，不得误写成 bundled `offsetPlc` coverage。S1 未新增 fixture、未采 native oracle、未改 cad-core source / capability / tests / expected。
- C9-M2 S2 已关闭范围准入：S2 live 基线为 `HEAD=87f289aaba`（`87f289aaba docs: 关闭 C9-M2 S1 源码候选矩阵`），起始 status 无输出；`C9M2-SCOPE-101/102/103` 路由到 S3 native oracle，`C9M2-SCOPE-201` 路由到 S4 expected activation，`C9M2-SCOPE-301/302` 路由到 S5 zero Angle known-gap/native oracle 与 diagnostics guard review，`C9M2-SCOPE-303/304` 路由到 S6 release gate，`C9M2-SCOPE-401/402` 和 `C9M2-NG-001..006` 保持 diagnostic non-goal / forbidden claims。`C9M2-BLOCKER-201` 已关闭；S2 未改 cad-core source / fixtures / expected / tests，未采 native oracle。

## S3 关闭结论

- S3 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=f500c34407`（`f500c34407 docs: 关闭 C9-M2 S2 范围准入矩阵`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- 已采集三条 bundled `offsetPlc` native expected：`assembly-bundled-offset-object-marker-real-solver.freecad.json`、`assembly-bundled-offset-subshape-marker-real-solver.freecad.json`、`assembly-bundled-offset-placement-writeback-real-solver.freecad.json`。
- 三条 expected 均证明 `offsetPlc=[2,0,0]` 非 identity；object/subshape marker 证明 `data.offsetPlc * plc`，writeback 证明 `getMbdPlacement(mbdPart) * offsetPlc`，`ComponentC` expected writeback 为 `[6,0,2]`。
- 当前 cad-core 对三条 fixture 的 `ComponentC` writeback 仍为 `[4,0,0]`，与 native expected mismatch；`C9M2-SCOPE-101/102/103` 和 `C9M2-BG-101/102/103` 均路由为 `backend_gap_candidate`，仅交 S6 消费。S3 未修改 cad-core C++ solver 语义。

## S4 关闭结论

- S4 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=fe1b38727b`（`fe1b38727b feat(cad-core): 采集C9-M2 S3 bundled offset oracle`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- `assembly-marker-custom-placement-chain-real-solver` 已进入 `cad-core/tests/test_p8_features.py::CadCoreP8FeatureTest.test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch`，不再只作为 expected 库存证据。
- S4 focused test 直接断言该 fixture 的 `native_marker_oracle`、known_gap/backendGap retention、`FixedJoint` native handle-one-side evidence，以及每个 native/solver reference 的 `offset_boundary=identity_offset_for_two_box_assembly_link_fixture`。
- S4 结论是 custom placement-chain expected activation；该 expected 的 boundary 是 identity offset，不代表 S3 non-identity bundled `offsetPlc` coverage，也不关闭 S3 backend_gap_candidate。`C9M2-SCOPE-201`、`C9M2-BG-201` 和 `C9M2-BLOCKER-401` 已关闭为 focused test activated。

## S5 关闭结论

- S5 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=1e67ff8971`（`1e67ff8971 test(cad-core): 激活C9-M2 S4 custom placement测试`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- `cad-core/fixtures/c3m6/expected/assembly-angle-zero-and-signed-current-real-solver.freecad.json` 已用 `/home/user/.local/bin/freecadcmd` 采集；默认 macOS `FreeCADCmd` 路径不可用，但 PATH 内 native FreeCADCmd 可用，最终采集成功。expected 记录 FreeCADCmd 1.2.0 revision 20260519、native solver return 0、`AngleZeroJoint.angle=0.0`、native current XY angle 0、`ComponentB` native placement update `[4,0,4]`。
- current cad-core 对同 fixture 保留 `Angle=0` solver DTO 与 resolved subshape marker evidence，但 real Ondsel solve 返回 `ondsel_solver_failed` / `iterNo > iterMax`，无 placement updates；`C9M2-SCOPE-301` 与 `C9M2-BG-301` 已路由为 expected-backed `backend_gap_candidate`，只交 S6，不在 S5 改 C++。
- `cad-core/tests/test_p8_features.py::CadCoreP8FeatureTest.test_c3m6_assembly_zero_angle_s5_native_oracle_routes_current_gap` 锁定 native expected 与 current mismatch；`test_c9m2_s5_assembly_solver_diagnostics_remain_visible` 锁定 unsupported JointType、PointCurve/default boundary、missing grounded 仍走 diagnostics，zero-angle fixture 同时证明 `ondsel_solver_failed` 可见；`test_adapters.py` 继续断言 capability 发布 `invalid_assembly_solver_result`。
- `C9M2-SCOPE-302`、`C9M2-BG-302` 和 `C9M2-BLOCKER-501` 已关闭为 diagnostics guard reviewed；S6 状态不变。

## S6 关闭结论

- S6 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=7f07180da0`（`7f07180da0 feat(cad-core): 关闭C9-M2 S5 zero angle复审`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- 已按 FreeCAD authority 关闭 S3-S5 的 `backend_gap_candidate`：`getMbDData()` 生产 request-local bundled `offsetPlc`，`handleOneSideOfJoint()` 消费 `data.offsetPlc * plc` 解析 marker，`setNewPlacements()` / `validateNewPlacements()` 消费 `getMbdPlacement(mbdPart) * offsetPlc` 转回 document placement；Angle 0 fallback 对齐 `makeMbdJointOfType()` 的 parallel-axes native route。
- focused tests 已覆盖三条 bundled `offsetPlc` expected、S4 custom placement-chain expected、S5 zero Angle expected-backed current parity，以及 unsupported JointType、PointCurve/default boundary、missing grounded 和 `invalid_assembly_solver_result` diagnostics guard。
- capability 已把 bundled offset marker/writeback 纳入 covered request-local subset；`non_identity_bundled_offsetPlc` 不再作为 non-goal 发布，`non_assembly_link_subshape_primitive_frame_generalization` 保持 diagnostic non-goal。
- S6 验证已通过：C9-M2 queue 为空、TSV 字段数一致、尾随空白检查无匹配、`git diff --check` 通过，`cmake --build build` 与 `python3 -m unittest tests.test_p8_features tests.test_adapters` 通过，`./cad-core capabilities` 仍发布 `assembly.remaining_gaps=[]`。
- `C9M2-BLOCKER-601` 已关闭；S6 文件重命名为 `【已实现】` 后，本批次队列为空。

## 验收分层

文档短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次 docs/CADCore9.0/README.md
git diff --check
```

实现闸门由 S6 按矩阵裁决选择：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
./cad-core capabilities > /tmp/c9m2-capabilities.json
```
