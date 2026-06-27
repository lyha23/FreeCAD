# C8-M7 S0 live 基线与 residual 声明冻结

## 目标

冻结 C8-M7 live 起点，确认本包只处理 `import_shape.changed_file_deleted_reference_recovery` 残留，不重开 C7-M7 已关闭的持久 Link / imported ElementMap / cross-document 生命周期。

## 输入

- `docs/CADCore8.0/README.md`
- `docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/README.md`
- `docs/CADCore7.0/C7-M7-P8LinkElement持久写回与导入ElementMap完整闭环主线/README.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/tests/test_adapters.py`
- `../矩阵/*.tsv`

## 必须复核

- `pwd` 是 `/home/user/Chili3DProject/FreeCAD`。
- `git rev-parse --short=10 HEAD` 与 `git log -1 --oneline` 写回 README 和 blocker 矩阵。
- C8-M1 到 C8-M6 工作步骤队列为空，C8-M7 队列首项为 S0。
- `capability_contract.cpp` 仍发布 `import_shape.status=done_first_slice`、`covered=["step","brep","iges","owner_qualified_alias"]`、`remaining=["changed_file_deleted_reference_recovery"]`。
- C7-M7 的完整 imported ElementMap / ShowElement persistent writeback / cross-document hash lifecycle 仍为 inherited non-goal 或 oracle-blocked，不进入 C8-M7 实现范围。

## 必须回写

- `C8M7-BLOCKER-000`
- `C8M7-SCOPE-001`
- `C8M7-SCOPE-101`
- `C8M7-NG-001` 到 `C8M7-NG-007`
- `C8M7-VAL-001` 到 `C8M7-VAL-004`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git -c core.quotepath=false status --short -uall
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M6-ShapeBinderSubShapeBinder下游同步源头合同主线/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M7-ImportShapeChangedFileDeletedReferenceRecovery准入收口主线/工作步骤细分 --format markdown
rg -n 'import_shape|changed_file_deleted_reference_recovery|owner_qualified_alias' cad-core/src/runtime/capability_contract.cpp cad-core/tests/test_adapters.py
```

## 非目标

- 不采 FreeCAD oracle。
- 不修改 `cad-core/src`、fixtures、expected 或测试。
- 不跑重型阶段回归。
