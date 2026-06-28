# 【已实现】C9-M5-S0 live 基线与 known-gap 声明口径冻结

## 目标

冻结 C9-M5 起点：当前 HEAD、dirty state、C9-M1 到 C9-M4 队列状态、live capability 中 SubShapeBinder CopyOnChange known gap 的发布口径，以及本包 forbidden claims。S0 不采 oracle、不改 C++、不新增 fixture。

## 执行基线

- `pwd`：`/home/user/Chili3DProject/FreeCAD`
- `git rev-parse --short HEAD`：`ceef6a128b`
- `git log -1 --oneline`：`ceef6a128b feat: 关闭 C9-M4 S6 默认距离类型发布闸门`
- `git -c core.quotepath=false status --short -uall`：仅显示 `docs/CADCore9.0/README.md` 修改和本 C9-M5 包未跟踪文件；未发现范围外 dirty state。

## 输入

- `docs/CADCore9.0/README.md`
- `cad-core/src/runtime/capability_contract.cpp`
- `cad-core/src/part_design/feature_shape_binder.cpp`
- `cad-core/tests/test_c8_shapebinder.py`
- `docs/CADCore8.0/C8-M2-SubShapeBinderCopyOnChange下游同步与DTO准入主线/`

## 声明口径

| 项 | S0 口径 |
| --- | --- |
| C9-M4 状态 | 队列为空，Assembly 不再有 active remaining gap。 |
| C9-M5 目标 | 复审 SubShapeBinder CopyOnChange request-local DTO 准入。 |
| 当前 gap | `part_design.sub_shape_binder.copy_on_change_full_temporary_document_cache`。 |
| 当前状态 | `known_gap_diagnostic` / `oracle_blocked`。 |
| S6 code gate | 仅在 S3 native evidence + S4 product boundary 都成立时打开。 |

S0 冻结结论：C9-M4 queue-empty 是 C9-M5 的 live 起点；`copy_on_change_full_temporary_document_cache` 仍是 retained known gap，不是 supported，也不是 `backend_gap_requires_implementation`。S6 不允许绕过 S3/S4 直接落 C++。

## 禁止声明

- 不得声明 full temporary-document copied-object cache supported。
- 不得把 Disabled / Enabled / Mutated / PartialLoad property-state evidence 等同于 copied-object DTO。
- 不得引入跨请求 backend session、persistent temporary document、BREP、TopoDS_Shape、NamedShape、ElementMap 或 cache。
- 不得把 C8-M2 no-code 裁决删除，除非本包记录更强 native evidence 和产品边界。

## 必须回写的矩阵行

- `C9M5-SCOPE-001`
- `C9M5-SCOPE-101`
- `C9M5-SCOPE-102`
- `C9M5-BLOCKER-000`
- `C9M5-NG-001` 到 `C9M5-NG-006`

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
git status --short -uall
git log -1 --oneline
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M4-AssemblyDistanceTypeDefaultMissingOracle扩面批次/工作步骤细分 --format markdown
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M5-SubShapeBinderCopyOnChangeRequestLocalDTO准入复审包/工作步骤细分 --format markdown
cd cad-core && ./cad-core capabilities > /tmp/c9m5-capabilities.json
```

验收标准：

- C9-M4 队列输出只有 Markdown 表头。
- C9-M5 队列下一项为本 S0。
- `/tmp/c9m5-capabilities.json` 中 `part_design.sub_shape_binder.remaining_gaps` 仍只包含 `copy_on_change_full_temporary_document_cache`。
- 本文件、README、总入口和矩阵中没有把该 gap 写成 supported 或 backendGap requires implementation。

## 非目标

- 不修改 `cad-core/src`、fixtures、expected 或 tests。
- 不采集 FreeCADCmd oracle。
