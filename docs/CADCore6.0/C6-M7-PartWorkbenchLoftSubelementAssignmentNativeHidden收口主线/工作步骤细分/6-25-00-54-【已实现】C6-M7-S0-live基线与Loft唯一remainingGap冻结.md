# 【已实现】C6-M7-S0 live 基线与 Loft 唯一 remainingGap 冻结

## 目标

冻结 C6-M7 的 live baseline：确认 C6-M1 到 C6-M6 队列均已关闭，`part_workbench.loft` 只有 `part_loft_subelement_assignment_native_hidden` 一个 active remaining gap，并读取 C5-M12 Loft diagnostic expected。

## 已冻结基线

- `pwd=/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD=5caad308a9`。
- `git log -1 --oneline=5caad308a9 发布 C6-M6 GeomPlate release gate`。
- S0 开始前 `git -c core.quotepath=false status --short -uall` 仅显示 `docs/CADCore6.0/README.md` 与 C6-M7 文档包未提交；本步继续只改这些文档和矩阵。
- C6-M1 到 C6-M6 `step_goal_queue.py` 均返回空表。
- S0 开始时 C6-M7 队列从 S0 到 S5；S0 完成并重命名后队列应从 S1 继续。

## Loft capability 证据

- `cad-core/src/runtime/capability_contract.cpp` 的 `part_workbench.loft` capability：`status=supported_profile_linearize_complex_expected_backed`，`fixtures` 包含 `c5m12/part-loft-subelement-assignment-diagnostic`，`request_local_boundaries` 包含 `sketch_subelement_assignment_native_hidden`，`remaining_gaps=["part_loft_subelement_assignment_native_hidden"]`。
- `cad-core/tests/test_adapters.py` 对同一 capability 断言 `loft["remaining_gaps"] == ["part_loft_subelement_assignment_native_hidden"]`，并确认 `complex_profile_family`、`geomplate` 和 `full_part_surface_family` 不在 Loft active remaining gaps 中。

## C5-M12 diagnostic expected 证据

- `cad-core/fixtures/c5m12/expected/part-loft-subelement-assignment-diagnostic.freecad.json` 记录 `known_gap.kind=part_loft_subelement_assignment_native_hidden`。
- `freecadcmd_evidence.error=TypeError: Type must be App.DocumentObject or None, not tuple`，`property=Sections`，`source=C5-M12-S1 loft_profiles probe`。
- `uncollected_fields` 为 `object_fields.sections[].subname` 和 selected Sketch subelement `shape_summary`。
- `delete_condition` 仍是：只有 upstream `Part::Loft.Sections` 暴露 `App::PropertyLinkList` native subelement storage，或 C5/C6 scope 批准 request-local DTO 且不当作 FreeCAD native expected 时，才删除该 known gap。

## 产物

- 已更新 C6-M7 README / 总入口中的 baseline。
- 已填实 scope、blocker、validation 等矩阵中的 S0 actual evidence。
- 不改 C++、capability、build 或 fixture。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git -c core.quotepath=false status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线/工作步骤细分 --format markdown
rg -n 'part_loft_subelement_assignment_native_hidden|part_workbench\\.loft' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py cad-core/fixtures/c5m12/expected/part-loft-subelement-assignment-diagnostic.freecad.json
git diff --check -- docs/CADCore6.0/C6-M7-PartWorkbenchLoftSubelementAssignmentNativeHidden收口主线 docs/CADCore6.0/README.md
```

## 非目标

- 不跑构建。
- 不改 capability。
- 不创建 C6-M7 fixture。
