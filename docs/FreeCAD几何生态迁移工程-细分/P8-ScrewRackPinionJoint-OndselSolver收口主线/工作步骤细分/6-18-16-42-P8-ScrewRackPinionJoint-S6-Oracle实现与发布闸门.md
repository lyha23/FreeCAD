# P8 ScrewRackPinionJoint S6 Oracle 实现与发布闸门

## 目标

复核 S5 发布后的剩余边界：确认 Screw / RackPinion 已作为 request-local scalar JointType 支持发布，并确认 complex Distance、GUI/session 和 full transaction 未被误发布。

## 输入

- `SRJ-BLOCK-007`
- S5 已关闭的 `SRJ-BLOCK-002` / `SRJ-BLOCK-005` / `SRJ-BLOCK-006`
- FreeCAD source authority：`AssemblyObject::makeMbdJointOfType()`、`AssemblyObject::makeMbdJoint()`、`slidingPartIndex()`、`swapJCS()`、`getRackPinionMarkers()`
- c3m6 fixture / expected
- hard-linked real Ondsel build

## 实施顺序

1. 复核 `SRJ-BLOCK-001` 到 `SRJ-BLOCK-006` 均已由 S3-S5 关闭，且不需要重新实现。
2. 复核 `SRJ-BLOCK-007`：complex Distance 保持 `notCollected`；GUI/session/full transaction 保持 `nonGoal`。
3. 如发现发布文字误把 scalar `Distance` 扩成 full `DistanceType`，只修正文档 / capability 声明，不新增代码特判。

## 下一轮复核落点

| blocker | C++ / 测试 / 文档落点 | 成功标准 |
| --- | --- | --- |
| `SRJ-BLOCK-001` | 已由 S3 关闭 | request-local helper 能从同一 Assembly 的 Slider joint 计算 sliding side；不满足条件时输出 diagnostic，不猜测 |
| `SRJ-BLOCK-002` | 已由 S5 关闭 | Screw 映射到 `ASMTScrewJoint`，`pitch=Distance`，`solver_joints` 暴露 pitch / sliding evidence |
| `SRJ-BLOCK-003` | 已由 S4 关闭 | RackPinion 映射到 `ASMTRackPinionJoint`，`pitchRadius=Distance`，`solver_joints` 暴露 pitch_radius |
| `SRJ-BLOCK-004` | 已由 S4 关闭 | RackPinion marker rewrite 发生在 Ondsel marker 创建前，符合 FreeCAD rack / pinion side 和 yaw adjustment 依据 |
| `SRJ-BLOCK-005` | 已由 S5 关闭 | Screw / RackPinion fixture 和 FreeCADCmd expected 入库，focused parity pass |
| `SRJ-BLOCK-006` | 已由 S5 关闭 | `supported_joint_matrix` 与 focused tests 一致，`unsupported_joint_matrix` 不保留 stale Screw / RackPinion |
| `SRJ-BLOCK-007` | 本包 TSV、P8 AssemblySolver nonGoal / backend gap TSV | complex Distance 保持 notCollected，GUI/session 和 full transaction 保持 nonGoal |

## 禁止路径

- 禁止恢复 representative fallback 或 unlinked build support。
- 禁止靠 fixture 名称、bbox、volume、输出顺序、shape 数量或 component 名称推断 Screw / RackPinion 业务语义。
- 禁止在 adapter 层补业务语义或输出端修正 marker。
- 禁止把 complex Distance geometry、GUI drag、postDrag、Reverse UI 或 persistent session 混入本包。
- 禁止把 `swapJCS()` 实现成持久 DocumentObject graph 改写；只能影响本次 solver DTO / marker input。

## 验收标准

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_grounded_joint_matrix_uses_real_ondsel_solver
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
git diff --check -- ../docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线 src/assembly/joint_solver.cpp include/cad_core/assembly/joint_solver.h src/adapters/c_api/c_api.cpp tests/test_p8_features.py tests/test_adapters.py fixtures/c3m6
```

native oracle gate：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-screw-joint-real-solver.json --check
FREECADCMD=${FREECADCMD:-FreeCADCmd} python3 tools/collect_freecad_expected.py fixtures/c3m6/assembly-grounded-rackpinion-joint-real-solver.json --check
```

发布前 TSV 检查：

```bash
for f in /home/user/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/P8-ScrewRackPinionJoint-OndselSolver收口主线/矩阵/*.tsv; do
  awk -F '\t' 'FNR==1{n=NF} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF}' "$f"
done
```

关闭条件：

- `SRJ-SCOPE-002/003/004/005/006` 全部从 unsupported / notCollected / releaseGate 转为 `supported`，且证据记录在本文件或后续 `【已实现】` S6 文件中。
- `SRJ-SCOPE-007` 保持 `notCollected`，`SRJ-SCOPE-008` 保持 `nonGoal`。
- `git status --short` 中只包含本轮相关文件，且无 build 产物 / `__pycache__` 混入。

## 非目标

- 不运行全量 FreeCAD 构建。
- 不实现完整 Distance geometry matrix。
- 不调整 unrelated P5/P6/P7 docs 或 matrices。
