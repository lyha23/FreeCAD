# 【已实现】C9-M2 S4 customPlacementChain 测试激活

## 目标

把 C9-M1 发现的 existing expected `assembly-marker-custom-placement-chain-real-solver` 接入 focused tests，避免 native expected 只作为未被断言的库存证据。

## FreeCAD 依据

- `src/Mod/Assembly/App/AssemblyObject.cpp::AssemblyObject::handleOneSideOfJoint()`
- `cad-core/fixtures/c3m6/expected/assembly-marker-custom-placement-chain-real-solver.freecad.json`
- `cad-core/tests/test_p8_features.py`

## 必须复核

- expected 中的 non-identity connector / part placement chain 与 bundled `offsetPlc` 不混淆。
- focused test 应明确断言 `offset_boundary=identity_offset_for_two_box_assembly_link_fixture`，防止误宣称 bundled offset coverage。
- current cad-core marker evidence 与 expected 的 native marker oracle 对齐。
- 如果 expected schema 不足以断言，记录缺口并把它路由到 S6 的 fixture/test schema patch，而不是忽略。

## 必须回写的矩阵行

- `C9M2-SCOPE-201`
- `C9M2-BG-201`
- `C9M2-BLOCKER-401`
- 必要时 `C9M2-VAL-401`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'assembly-marker-custom-placement-chain-real-solver|offset_boundary|custom placement|native_marker_oracle|markerResolutionStatus' cad-core/fixtures/c3m6 cad-core/tests/test_p8_features.py docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次
cd cad-core && python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch
cd /home/user/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M2-AssemblyRequestLocalSolverOracle批次/矩阵/*.tsv
git diff --check
```

S4 关闭时，custom-chain expected 必须进入 focused tests，或记录为什么 schema / oracle 不足并交给 S6 修复；不得把它误写成 bundled `offsetPlc` expected。

## S4 关闭结论

- S4 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=fe1b38727b`（`fe1b38727b feat(cad-core): 采集C9-M2 S3 bundled offset oracle`），起始 `git -c core.quotepath=false status --short -uall` 无输出。
- `cad-core/tests/test_p8_features.py::CadCoreP8FeatureTest.test_c3m6_assembly_marker_placement_s4_native_oracle_expected_batch` 已直接命中 `assembly-marker-custom-placement-chain-real-solver`，读取现有 expected，不再只依赖 expected 库存扫描。
- focused test 断言该 expected 是 custom placement-chain evidence：fixture 输入含非 identity AssemblyLink placements，expected 保留 known_gap/backendGap，`native_marker_oracle.requires_cad_core_marker_parity=true`，`FixedJoint` 四个 native / solver reference 均为 `resolved_native_handle_one_side`。
- focused test 明确断言四个 reference 的 `offset_boundary=identity_offset_for_two_box_assembly_link_fixture`，因此 S4 只关闭 custom-chain expected activation；它不是 S3 non-identity bundled `offsetPlc` parity，也不修 S3 backend_gap_candidate。
- `C9M2-SCOPE-201`、`C9M2-BG-201` 和 `C9M2-BLOCKER-401` 已关闭为 focused test activated；S5-S6 状态不变。

## 非目标

- 不采新的 bundled offset oracle。
- 不把 identity offset boundary 改写成 non-identity bundled offset coverage。
- 不用 adapter 输出字符串替代 focused test。
