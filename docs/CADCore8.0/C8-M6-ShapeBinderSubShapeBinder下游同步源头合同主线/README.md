# C8-M6 ShapeBinder/SubShapeBinder 下游同步源头合同主线

## 定位

C8-M6 不修改下游仓库，也不新增 `cad-core` 几何能力。它承接 C8-M1、C8-M2 和 C8-M5 的已验证结论，把 `PartDesign::ShapeBinder` / `PartDesign::SubShapeBinder` 的下游同步源头合同重新整理成可执行包。

本包只回答一件事：下游同步时应该以 FreeCAD 仓库里的哪些 TypeId、capability、diagnostic、fixture expected、ElementMap / NamedShape 输出和 request-local 边界为准。

## 当前基线

- 仓库：`/home/user/Chili3DProject/FreeCAD`
- S0 live 基线提交：`9361ddc83a`（`docs: 新增 C8-M6 下游同步源头合同包`）
- S0 开始工作区干净：`git -c core.quotepath=false status --short -uall` 无输出；C8-M1 到 C8-M5 工作步骤队列均为空，C8-M6 队列首项为 S0。
- current capability：`part_design.shape_binder.status=supported_c8m1_expected_backed_request_local` 且 `remaining_gaps=[]`；`part_design.sub_shape_binder.status=supported_c8m1_expected_backed_request_local_with_copy_on_change_known_gap` 且唯一 `remaining_gaps=["copy_on_change_full_temporary_document_cache"]`。
- C8-M5 已恢复 C8-M1 expected fixture 阶段回归：`BodyBaseFeature` stale expected 已刷新，`SubShapeBinder Support` self-link 诊断已落为 `cycle_rejected_by_property_link`；旧 C8-M2 合同不能覆盖这两个最终口径。
- 本包 S0 只冻结文档和 TSV 同步声明；不修改 `cad-core/src`、fixtures、expected 或测试。

## 与既有 C8 包的关系

- C8-M1 是 ShapeBinder/SubShapeBinder executor、ElementMap / NamedShape 和 fixture expected 的能力闭环。
- C8-M2 是 CopyOnChange DTO 准入与下游同步源头合同首版；它明确不把 full temporary-document cache 标成 supported。
- C8-M5 是 C8-M1 expected drift 的阶段回归恢复；本包的合同必须吸收 C8-M5 的最终口径，不能继续引用旧的 `BodyBaseFeature` stale expected 或 generic `cycle_dependency` 结论。

## 主文件

- 总入口：`6-27-11-28-C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线总入口.md`
- 方案：`6-27-11-28-C8-M6-ShapeBinderSubShapeBinder下游同步源头合同方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 验收分层

本轮短跑默认验收：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线 docs/CADCore8.0/README.md
git diff --check
```

阶段合同复核：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core capabilities > /tmp/c8m6-capabilities.json
python3 -m unittest tests.test_c8_shapebinder tests.test_diagnostics tests.test_adapters
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest.test_expected_fixtures_match_recompute_results
```

发布闸门：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters tests.test_diagnostics
```
