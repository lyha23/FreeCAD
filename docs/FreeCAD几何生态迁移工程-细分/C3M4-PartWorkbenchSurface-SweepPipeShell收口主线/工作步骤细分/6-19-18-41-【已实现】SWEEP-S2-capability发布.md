# 【已实现】SWEEP-S2 capability 发布

更新 CADCore 文档、专题包矩阵、C ABI capability metadata 和 adapter tests。只发布 S1 已验证的 source-backed `Part::Sweep` / PipeShell first batch，不扩展 executor 语义、不新增 fixture、不重采 expected。

## live 基线

- `pwd`：`/Users/li/Chili3DProject/FreeCAD`。
- `git rev-parse --short HEAD`：`606611ae27`。
- `git log -1 --oneline`：`606611ae27 feat: 实现 Part Sweep 执行器`。
- `git -c core.quotepath=false status --short -uall`：起始工作区干净。

## 发布边界

- C ABI `part_workbench.sweep` 发布状态为 `supported_expected_backed_first_batch`。
- TypeId：`Part::Sweep`。
- 属性：`Sections`、`Spine`、`Solid`、`Frenet`、`Transition`、`Linearize`。
- Property types：`App::PropertyLinkList`、`App::PropertyLinkSub`、`App::PropertyBool`、`App::PropertyEnumeration`。
- Covered：source-backed executor、`Spine` SubList compound、one profile、`Solid/Frenet/Transition`、PipeShell maker history、expected-backed fixtures、invalid diagnostics。
- Topo history 发布 `pipeshell` maker-history producer，并在 producer matrix 中固定 PipeShell first batch。
- Loft remaining gap 不再使用 `sweep_filling_geomplate_pipeshell` 这类把 Sweep 继续归为未完成缺口的口径。

## Fixture 清单

- `c3m4/part-sweep-right-corner-surface`
- `c3m4/part-sweep-solid`
- `c3m4/part-sweep-frenet-off`
- `c3m4/part-sweep-transition-transformed`
- `c3m4/part-sweep-transition-round-corner`
- `c3m4/part-sweep-spine-subedges`
- `c3m4/part-sweep-open-profile-surface`
- `c3m4/part-sweep-invalid-inputs`

## 非目标

- 不发布 `Linearize=true` post-processing。
- 不发布 advanced `BRepOffsetAPI_MakePipeShell` wrapper。
- 不发布 auxiliary spine、located profile、support mode、trihedron/binormal mode。
- 不把 Hole `ModelThread` 内部 PipeShell 等同为 `Part::Sweep` 支持。
- 不声明 full Part surface family 完成。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_adapters.AdapterContractTest
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest

cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore方案 docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/FreeCAD几何生态迁移工程-细分/C3M4-PartWorkbenchSurface-SweepPipeShell收口主线/工作步骤细分 --format markdown
```

完成状态：本文件已按完成规则命名为 `6-19-18-41-【已实现】SWEEP-S2-capability发布.md`。
