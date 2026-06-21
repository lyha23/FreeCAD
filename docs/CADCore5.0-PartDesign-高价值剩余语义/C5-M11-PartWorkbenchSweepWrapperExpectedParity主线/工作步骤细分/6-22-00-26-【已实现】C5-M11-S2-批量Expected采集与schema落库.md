# 【已实现】C5-M11-S2 批量 expected 采集与 schema 落库

状态：`done_C5M11-S2_expected_batch_with_narrowed_blockers`

## live baseline

```text
pwd
/Users/li/Chili3DProject/FreeCAD

git rev-parse --short HEAD
447223b908

git log -1 --oneline
447223b908 feat: 完成C5-M11-S1 Sweep wrapper collector设计

git -c core.quotepath=false status --short -uall
 M cad-core/include/cad_core/part_design/body_topo_shape.h
 M cad-core/src/part_design/body.cpp
 M cad-core/src/runtime/recompute.cpp
 M cad-core/tests/test_adapters.py
?? docs/BUG修改/6-22-00-46-Fillet后Body面引用无法映射修复方案.md
```

上述脏文件是非本轮文件；S2 未暂存、覆盖或回退它们。

## S2 结论

- 已用同一 request-local `Part.BRepOffsetAPI_MakePipeShell` collector 批量尝试六个 C5-M10 wrapper expected。
- `part-sweep-auxiliary-spine-contract`、`part-sweep-binormal-contract`、`part-sweep-tolerance-contract` 已替换 broad `known_gap`，落为 FreeCADCmd wrapper expected：包含 `shape_summary`、顶层 cad-core 可比 `object_fields.advanced`，以及 `wrapper_oracle` 中的 `helper`、`runtime_helper`、`dto`、`builder_status` 和 wrapper-only sections metadata。
- `part-sweep-support-mode-diagnostics` 保留为缩窄 blocker：当前 fixture 是 diagnostic-only，没有 valid `SpineSupport` representative，未采字段是 `spine_support/support_mode/shape_summary`；下一批应新增或复用 valid support representative。
- `part-sweep-located-profile-contract` 保留为缩窄 blocker：FreeCADCmd 已解析 `add(Profile, Location, WithContact, WithCorrection)` 的输入，但 `build()` 报 `OCCError: NCollection_Array1::Value`，未采字段是 located section metadata 与 `shape_summary`。
- `part-sweep-advanced-combined-contract` 保留为缩窄 blocker：valid `CombinedSweep` 组合 auxiliary + located profile + tolerance 时同样触发 `OCCError: NCollection_Array1::Value`；invalid siblings 仍归 cad-core focused diagnostics。
- support diagnostic-only 与 located/combined blocker 都只影响对应 wrapper 子路径，不影响 auxiliary、binormal、tolerance 三个已收集 expected。

## 产物

- 更新 `cad-core/fixtures/c5m10/expected/part-sweep-auxiliary-spine-contract.freecad.json`
- 更新 `cad-core/fixtures/c5m10/expected/part-sweep-binormal-contract.freecad.json`
- 更新 `cad-core/fixtures/c5m10/expected/part-sweep-tolerance-contract.freecad.json`
- 缩窄 `cad-core/fixtures/c5m10/expected/part-sweep-support-mode-diagnostics.freecad.json`
- 缩窄 `cad-core/fixtures/c5m10/expected/part-sweep-located-profile-contract.freecad.json`
- 缩窄 `cad-core/fixtures/c5m10/expected/part-sweep-advanced-combined-contract.freecad.json`
- 更新 `cad-core/tools/collect_freecad_expected.py` 的 wrapper expected schema 与 `--check` 对比。
- 更新 `cad-core/tests/test_p8_features.py` 的 S2 expected/blocker 断言。
- 更新 `C5M11-BLK-201`、`C5M11-SCOPE-201`、`C5M11-ORC-201` 和 root `C5-ORC-1103` 状态。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 tools/collect_freecad_expected.py --phase c5m10 --check --skip-unsupported
# processed=3 skipped=3 failed=0

python3 -m unittest tests.test_expected_fixtures tests.test_p8_features
# Ran 194 tests in 87.459s
# OK (skipped=28)
```
