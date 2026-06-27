# C9-M2 Assembly request-local solver oracle 批次

## 定位

C9-M2 承接 C9-M1 no-code closure 后的剩余 Assembly request-local solver evidence。它不把后续工作压缩成单个 bundled `offsetPlc` fixture，而是把同一 FreeCAD 调用链里的 oracle、focused tests、capability 和可能的 C++ 落点一次拆清。

## 当前状态

- S0 live 基线：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=d52cd67a19`（`d52cd67a19 docs: 关闭 C9-M1 S6 发布闸门`）。S0 起始 status 仅包含 `docs/CADCore9.0/README.md` 与本 C9-M2 seed 文档 / 矩阵 / step 文件，未出现 cad-core source、fixture、expected 或 tests 改动。
- C9-M1 队列为空，C9-M1 已关闭且不重开；C9-M2 只处理 Assembly request-local solver oracle 批次。
- live capability 仍发布 `assembly.remaining_gaps=[]`、`subshape_marker_placement.remaining_gaps=[]`，`assembly.ondsel_solver_adapter.status=covered_full`，`placement_writeback.status=covered_full`。
- `non_identity_bundled_offsetPlc` 仍是 oracle candidate / forbidden guessing，因为缺 fixed-joint bundle 产生 non-identity `objectPartMap.offsetPlc` 的 native expected。
- `assembly-marker-custom-placement-chain-real-solver` 已有 expected，但 C9-M1 记录它未被 focused tests 直接断言。
- `non_assembly_link_subshape_primitive_frame_generalization` 仍是 diagnostic non-goal；zero Angle fallback 有 FreeCAD / cad-core source evidence，但缺 native expected / focused test。

## 批次边界

| 方向 | 当前状态 | C9-M2 目标 |
| --- | --- | --- |
| bundled `offsetPlc` object marker | oracle_candidate | native expected + current compare |
| bundled `offsetPlc` subshape marker | oracle_candidate | native expected + current compare |
| bundled `offsetPlc` writeback | oracle_candidate | native expected + current compare |
| custom placement-chain expected | expected exists, test not direct | focused test activation |
| zero Angle fallback | known_gap_retained | native expected + focused test or retained route |
| primitive frame generalization | diagnostic_non_goal | 保持 non-goal，除非产品另批 DTO |

S0 关闭证据：C9-M1 queue script 只输出表头；C9-M2 queue 在 S0 重命名前从 S0 开始；`cad-core/src/runtime/capability_contract.cpp` 与 `cad-core/tests/test_adapters.py` 仍断言 Assembly remaining gaps 为空、`non_identity_bundled_offsetPlc` 和 primitive frame generalization 在 non-goals 内。本步未采 native oracle、未改 cad-core 源码 / fixtures / expected / tests，也未运行 build 或重型回归。

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
