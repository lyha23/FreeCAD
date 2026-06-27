# C9-M2 S1 FreeCAD 源码与 oracle 候选矩阵

## 目标

复核 bundled `offsetPlc`、custom placement-chain、zero Angle fallback 和 diagnostics guard 的 FreeCAD / cad-core source authority，形成 C9-M2 source candidates。

## 输入

- FreeCAD：`src/Mod/Assembly/App/AssemblyObject.cpp`
- FreeCAD：`src/Mod/Assembly/App/AssemblyObject.h`
- FreeCAD：`src/Mod/Assembly/App/AssemblyUtils.cpp`
- cad-core：`cad-core/src/assembly/joint_solver.cpp`
- cad-core：`cad-core/src/assembly/assembly_utils.cpp`
- cad-core：`cad-core/src/assembly/assembly_object.cpp`
- capability：`cad-core/src/runtime/capability_contract.cpp`
- tests / fixtures：`cad-core/tests/test_p8_features.py`、`cad-core/tests/test_adapters.py`、`cad-core/fixtures/c3m6`

## 必须复核

- `getMbDData()` 的 fixed-joint bundling、`objectPartMap` 和 `offsetPlc` 生成方式。
- `handleOneSideOfJoint()` 应用 `data.offsetPlc * plc` 的顺序。
- `validateNewPlacements()` / `setNewPlacements()` 应用 `getMbdPlacement(mbdPart) * offsetPlc` 的顺序。
- `makeMbdJointOfType()` zero Angle fallback 到 `ASMTParallelAxesJoint`。
- current `test_p8_features.py` 是否直接断言 custom placement-chain expected。
- capability 是否仍把 non-identity bundled `offsetPlc` 和 primitive frame generalization 作为 non-goals。

## 必须回写的矩阵行

- `C9M2-SRC-101` 到 `C9M2-SRC-403`
- `C9M2-BLOCKER-101`
- README 的 S1 结论段

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'offsetPlc|objectPartMap|getMbDData|handleOneSideOfJoint|runPreDrag|setNewPlacements|validateNewPlacements|makeMbdJointOfType|ASMTParallelAxesJoint|assembly-marker-custom-placement-chain-real-solver|non_identity_bundled_offsetPlc' src/Mod/Assembly/App cad-core/src/assembly cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_p8_features.py cad-core/tests/test_adapters.py cad-core/fixtures/c3m6
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
git diff --check
```

S1 关闭时，source candidates 必须都有 source evidence、cad-core landing 和 next step；不得把候选直接写成 supported。

## S1 关闭结论

- S1 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=8dc1ec2ccd`（`8dc1ec2ccd docs: 关闭 C9-M2 S0 基线冻结`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- `C9M2-SRC-101..403` 已回写 source evidence、cad-core landing 和 next action；`C9M2-BLOCKER-101` 已关闭为 `Closed S1`。
- FreeCAD source authority：`getMbDData()` 在 `bundleFixed` 下将 fixed-joint connected part 复用同一 `ASMTPart` 并记录 `plc.inverse() * plci`；`handleOneSideOfJoint()` 在 object-global 与 part-local 转换后执行 `data.offsetPlc * plc`；`validateNewPlacements()` / `setNewPlacements()` 使用 `getMbdPlacement(mbdPart) * offsetPlc`；`makeMbdJointOfType()` 对 Angle 0 或 2pi fallback 到 `ASMTParallelAxesJoint`。
- cad-core source authority：marker / Angle 落点在 `joint_solver.cpp`，writeback JSON 在 `assembly_utils.cpp`，request-local display apply 在 `assembly_object.cpp`，capability 与 adapter tests 仍把 `non_identity_bundled_offsetPlc` 和 primitive frame generalization 保持为 non-goals。
- `assembly-marker-custom-placement-chain-real-solver.freecad.json` 已存在，但 focused tests 尚未直接断言；S1 未新增 fixture、未采 native oracle、未改 cad-core source / capability / tests / expected，也未把候选写成 supported。

## 非目标

- 不新增 fixture。
- 不采 native oracle。
- 不改 capability。
- 不用 current cad-core 输出倒推 FreeCAD expected。
