# C12-M14 Part Sweep helper mutable lifecycle 证据解锁批次方案

## 目标

在 C12-M13 已完成 PartDesign Pipe 剩余语义迁移后，专门解锁 Part Workbench `BRepOffsetAPI_MakePipeShellPy` mutable helper 的剩余方法证据。C12-M14 不直接假设 C++ 实现成立；只有 native probe 或 product-contract artifact 覆盖方法集合、调用顺序、diagnostics 和 current response 字段后，才进入实现。

## 范围切分

### S0 live 基线与 C12-M13 继承冻结

冻结当前 `HEAD`、dirty boundary、C12-M13 final status 和 `ORACLE-301` 重开条件。确认本包只处理 helper uncollected methods，不混入 ORACLE-001 或 PartDesign Pipe 已关闭项。

### S1 source 与 current helper landing 复核

复核：

- `BRepOffsetAPI_MakePipeShellPyImp.cpp` 中 helper method binding 和异常路径。
- `PartFeatures.cpp::Sweep::execute()` wrapper no-mix 边界。
- `cad-core/src/part/part_sweep.cpp` 当前 wrapper / advanced DTO。
- `cad-core/src/part/topo_shape_expansion.cpp` shared builder 和已有 `Simulate(2)` 内部用途。
- C12-M13 `part-sweep-helper-mutable-sequence` fixture / expected / P8 focused test。

### S2 dedicated native helper probe schema

设计稳定 FreeCADCmd probe schema，只收集 helper lifecycle evidence，不把临时崩溃写成 supported。至少覆盖：

- `add -> isReady -> getStatus -> build -> shape -> makeSolid` baseline subset。
- `remove` 后 readiness/status/build 行为。
- `firstShape/lastShape` 在未 build、build fail、build success 三态的行为。
- `generated(profile)` 的参数、返回类型和失败诊断。
- `simulate(count)` 的 count、返回 shape 列表和失败诊断。
- `remove/readd/simulate/build` 组合是否稳定；若仍触发 `NCollection_Sequence::ChangeValue`，记录为 native instability blocker。

### S3 product contract 与 current mismatch 准入

基于 S2 artifact 做三闸门裁决：

1. native expected 是否稳定可 checked-in。
2. 若 native 不稳定，是否批准 CAD Core request-local product contract。
3. current `part_sweep.cpp` 是否存在与 expected/product contract 的真实 mismatch。

三者未同时成立时不进入 C++ 实现。

### S4 helper lifecycle 实现或 no-code 收口

若 S3 授权实现，允许修改 `part_sweep.cpp` 和必要 shared builder DTO，补 `c12m14` fixtures / focused P8 tests。若 S3 未授权，实现步骤关闭为 `no_code_retained_helper_blocker`，只更新 docs/matrix。

### S5 发布闸门

发布最终状态：`implementation_unlocked_helper_lifecycle`、`product_contract_published_helper_lifecycle` 或 `no_code_retained_helper_blocker`。同步 root README、package README、矩阵和 capability / adapter wording 评估结论。

## 实现顺序

1. 先锁 source/current landing，不从 current output 倒推 helper semantics。
2. 再采 native helper probe；probe 不稳定时写 blocker。
3. 再做 product contract 准入裁决。
4. 最后才决定是否修改 C++。

## 最小验证命令

文档短跑：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M14-PartSweepHelperMutableLifecycle证据解锁批次 docs/CADCore12.0/README.md
git diff --check
```

代码实现后 focused 验证候选：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c12m13_part_sweep_helper_mutable_sequence_supported_subset_matches_native_oracle
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest.test_c12m14_part_sweep_helper_mutable_lifecycle_matches_native_or_product_contract
```

## 非目标

- 不在 S0-S3 修改 `cad-core/src`。
- 不改 PartDesign Pipe S3/S4 已关闭实现。
- 不把 helper product contract 强塞进 `PartDesign::Pipe`。
- 不把 `topo_shape_expansion.cpp` 内部 `Simulate(2)` cap/sewing 逻辑当作 Python helper `simulate()` parity。
