# PARTSURF-S2 fixture 与 oracle 矩阵设计

## 目标

把 S1 的源码裁决转成可实现的 fixture / oracle / diagnostics 矩阵，优先保证 `Part::RuledSurface` edge 分支有 FreeCAD expected 保护。

## 必读

- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/6-19-18-21-C3M4-PartWorkbenchSurface-RuledProjection收口方案.md`
- `/Users/li/Chili3DProject/FreeCAD/docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/矩阵/part_surface_fixture_oracle_matrix.tsv`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tools/collect_freecad_expected.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/fixtures/p8`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`
- `/Users/li/Chili3DProject/FreeCAD/cad-core/tests/test_expected_fixtures.py`

## 工作内容

1. 更新 `part_surface_fixture_oracle_matrix.tsv`，把 S3 required fixture 固定为 line-line、conic-line、orientation-reversed、invalid-input。
2. 设计 FreeCAD expected collector 路径：优先创建 source-backed `Part::RuledSurface` object；若必须使用 `Part.makeRuledSurface()`，必须在矩阵里说明和 `RuledSurface::execute()` 的等价边界。
3. 明确 fixture JSON schema：`Part::RuledSurface` 应通过 `DocumentObject` / `Properties.Curve1` / `Properties.Curve2` / `Properties.Orientation` 表达，不通过 adapter 特例输出。
4. 对 wire/wire 和 ProjectOnSurface candidate 做 promote/defer 裁决，不得含糊写成 supported。

## 非目标

- 不实现 C++ executor。
- 不修改 CADCore3.0 capability supported 文案。
- 不要求全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

完成后把本文件重命名为 `6-19-18-24-【已实现】PARTSURF-S2-fixture与oracle矩阵设计.md`。
