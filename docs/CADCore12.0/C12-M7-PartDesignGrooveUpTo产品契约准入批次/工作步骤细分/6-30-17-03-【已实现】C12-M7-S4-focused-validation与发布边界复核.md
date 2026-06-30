# 【已实现】C12-M7 S4 focused validation 与发布边界复核

## 目标

复核 S3 的 expected/test/capability/docs 迁移是否一致，并确认没有误发布 FreeCAD parity 或 broad PartDesign 支持。

## 必读来源

- S0-S3 已实现文档
- 本包矩阵
- touched expected/test/capability/docs 文件

## 操作

1. 运行 focused tests。
2. 检查 capability JSON 中 `part_design.revolution_groove` 的 status、fixtures、diagnostics、narrowed gap / product contract wording。
3. 复核 non-goals：CopyOnChange、RuledSurface、full Groove family、geometry C++ parity 都未被误关闭。
4. 更新 validation / blocker / scope 矩阵。

## 本轮基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=11cf58bc8f`。
- `git log -1 --oneline=11cf58bc8f feat: 发布 C12-M7 Groove UpTo 产品诊断契约`。
- 起点 `git -c core.quotepath=false status --short -uall` 无输出。
- S4 执行前队列首项仍是本文件，S5 pending。

## 复核结论

- focused Groove test 通过，两个 C51M1 Groove UpTo fixtures 继续读取 expected-backed product diagnostic contract，`freecad_native_parity=false`，primary diagnostic 为 `BRepFeat_MakeRevol could not revolve profile up to face`，secondary diagnostic 为 `Could not revolve the sketch`。
- C API capability smoke 通过，`part_design.revolution_groove.status=supported_c12m7_groove_upto_product_diagnostic_contract`，`remaining_gaps=[]`，`exact_blockers={}`。
- `/tmp/c12m7-s4-capabilities.json` 显示 narrowed gap `partdesign_groove_upto_brepfeat_cut_native_failure.status=published_c12m7_product_diagnostic_contract`，`route=product_diagnostic_contract_non_parity`，fixtures 仍是 `c51m1/partdesign-groove-uptofirst-body` 与 `c51m1/partdesign-groove-uptoface-body`。
- product diagnostic contract 字段仍包含 `diagnostic_codes=["execution_failed","execution_failed"]`、locatable fields、`object_status={"Groove":"error","Body":"skipped"}`、`freecad_native_parity=false`、native failure note、delete condition 和 reopen condition。
- non-goals 仍未误关闭：CopyOnChange 保持 C12-M5 retained diagnostic / `oracle_blocked`；RuledSurface wire/wire 保持 `supported_wire_wire_expected_backed`；本包未发布 full Groove family；未把 geometry C++ parity 当作完成。
- 本步未修改 `cad-core/src/part_design/feature_revolved.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/include` 或 adapter runtime，未新增 expected，未改 S3 已迁移 contract 语义，未处理 S5 release gate。

## 输出

- `C12M7-BLOCKER-401` 关闭为 `closed_s4_focused_validation_passed`。
- `C12M7-CON-006` 关闭为 `closed_s4_non_parity_boundary_confirmed`。
- `C12M7-SCOPE-006` 关闭为 `closed_s4_geometry_implementation_not_authorized`。
- S4 focused validation 相关行标记为 `passed_s4` / `passed_s4_revalidated`；S5 release gate 仍保持 pending。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51m1_groove_upto_native_brepfeat_failures_are_exact_blockers
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_publication_smoke
cd /Users/li/Chili3DProject/FreeCAD
cad-core/build/cad-core capabilities >/tmp/c12m7-s4-capabilities.json
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M7-PartDesignGrooveUpTo产品契约准入批次/矩阵/*.tsv
git diff --check
```
