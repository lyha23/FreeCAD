# 【已实现】PARTSURF-S2 fixture 与 oracle 矩阵设计

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

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`7d1776d1d0`。
- `git log -1 --oneline`：`7d1776d1d0 docs: 完成PARTSURF S1源码裁决`。
- `git -c core.quotepath=false status --short -uall`：既有未暂存 `src/Mod/Sketcher/App/SketchObject.h`、`src/Mod/Sketcher/App/SketchObjectPyImp.cpp`；另有其它 Surface / CADCore 主线未跟踪 docs。S2 只修改并暂存本 RuledProjection 主线文件，不触碰这些既有改动。

## S2 结论

- S3 required fixtures 固定为 `part-ruled-surface-line-line`、`part-ruled-surface-conic-line`、`part-ruled-surface-orientation-reversed`、`part-ruled-surface-invalid-input`。
- fixture JSON schema 必须表达 source-backed `DocumentObject`：`TypeId` 为 `Part::RuledSurface`，`Properties.Curve1` / `Properties.Curve2` 使用 `App::PropertyLinkSub` 的 object link 和 `SubList` / `StableSubList` 指向源 `EdgeN`，`Properties.Orientation` 使用 `App::PropertyEnumeration` 的 `Automatic` / `Forward` / `Reversed`。禁止 adapter 特例直接输出 ruled face。
- FreeCAD expected collector 路径优先创建 native `Part::RuledSurface` object，复用现有 `set_property()` 对 `App::PropertyLinkSub` / `App::PropertyEnumeration` 的设置能力；S3 需要先把 `Part::RuledSurface` 加入 native type 支持并采集 expected。
- `part-ruled-surface-conic-line` 若无法把 PARTCONIC 的 request-local DTO edge materialize 成 native linked object，collector 才允许用 `Part.ArcOf*().toShape()` + line shape 调 `Part.makeRuledSurface()`。该路径只等价于 `RuledSurface::execute()` 已完成 link/subname resolve 后进入 `makeElementRuledSurface()` 的 edge/edge geometry；不覆盖 link diagnostics、DocumentObject provenance 或 source edge topo 追踪。
- wire/wire 不进入 S3：当前没有 source-backed collector、fixture input schema、shell/topo provenance 三项验收闭环，按 `deferred` 记录。后续只有补齐这三项后才能重新 promote。
- `Part::ProjectOnSurface` 只保留 S4 candidate；S2/S3 不新增 ProjectOnSurface fixture，不发布 supported 文案。
- 已同步更新 `part_surface_fixture_oracle_matrix.tsv`、`part_surface_scope_review_matrix.tsv`、`part_surface_blocker_queue.tsv`、`part_surface_non_goal_registry.tsv` 和总方案中的 S2 结论。

## 非目标

- 不实现 C++ executor。
- 不采集 FreeCAD expected。
- 不新增实际 fixture / expected 文件。
- 不修改 CADCore3.0 capability supported 文案。
- 不要求全量 FreeCAD build。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-RuledProjection收口主线/工作步骤细分 --format markdown
```

完成状态：本文件已按完成规则命名为 `6-19-18-24-【已实现】PARTSURF-S2-fixture与oracle矩阵设计.md`。
