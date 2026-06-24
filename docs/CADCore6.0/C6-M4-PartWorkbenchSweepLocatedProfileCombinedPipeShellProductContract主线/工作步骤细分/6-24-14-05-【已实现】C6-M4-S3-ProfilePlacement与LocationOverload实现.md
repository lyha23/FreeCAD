# 【已实现】C6-M4-S3 ProfilePlacement 与 LocationOverload 实现

## 目标

消费 S2 的 located profile 合同，实现 `SectionOptions[].Location` / profile placement 的 CAD Core non-parity product path，并保留 c5m10 FreeCADCmd wrapper known_gap guard。S3 不声明 FreeCAD parity，不删除 capability remaining gap，不实现 S4 combined auxiliary + tolerance。

## S3 live baseline

- live repo：`/home/user/Chili3DProject/FreeCAD`
- S3 live HEAD：`6ede975075`
- S3 live last commit：`6ede975075 docs: 完成 C6-M4 S2 located profile 合同冻结`
- 起始工作区：`git -c core.quotepath=false status --short -uall` 无输出。
- 队列入口：`step_goal_queue.py .../工作步骤细分 --format markdown` 在本步骤执行前显示 S3-S6 pending，S3 是当前首个未实现步骤。

## FreeCAD / CAD Core 依据

| authority | S3 结论 |
| --- | --- |
| `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp::add()` | FreeCAD wrapper 公开 `add(Profile, Location, WithContact, WithCorrection)` 并调用 OCCT `Add(s, v, ...)`；S2 证明该路径 FreeCADCmd build 阶段仍 `NCollection_Array1::Value`，因此 S3 只能发布 CAD Core product contract。 |
| `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()` | 标准 PipeShell 在 `Add()` / `Build()` 后通过 `makeElementShape(mkPipeShell, shapes, op)` 消费 maker history；S3 必须保留 `part_sweep:pipeshell_history`，不能做输出端 bbox/order fixup。 |
| `cad-core/include/cad_core/part/topo_shape_expansion.h` | `PipeShellSectionOption` 增加 `PipeShellProfilePlacement::AnchorLocationToSpineStartProductContract`，默认仍是 `OcctLocationOverload`，保护 c5m10 known_gap。 |
| `cad-core/src/part/topo_shape_expansion.cpp` | S3 product path 将 Location vertex 点作为 profile anchor，request-local 平移到 spine 起点后调用常规 `Add(profile, WithContact, WithCorrection)`，并给 NamedShape 增加 `part_sweep:location_product_contract_profile_placement`。 |
| `cad-core/src/part/part_sweep.cpp` | 解析显式 `SectionOptions[].ProfilePlacement=AnchorLocationToSpineStart` 后写 `contract=cad_core_product_contract`、`contract_provenance=cad_core_product_contract_non_parity` 和 `freecadcmd_location_overload_status=notCollected`；未显式选择 product path 的 located overload 仍按旧 known_gap 处理。 |

## 已实现产物

| 产物 | 状态 |
| --- | --- |
| `cad-core/fixtures/c6m4/part-sweep-located-profile-product.json` | 已新增，valid located profile + `WithContact/WithCorrection` 输出 product-contract shape、metadata、NamedShape history。 |
| `cad-core/fixtures/c6m4/part-sweep-located-profile-diagnostics.json` | 已新增，覆盖 missing target、invalid subname、non-vertex Location、multi-subname，不 fallback 到 no-location Add。 |
| `cad-core/fixtures/c6m4/part-sweep-located-profile-bool-diagnostics.json` | 已新增，覆盖 malformed `WithContact/WithCorrection` 的 `invalid_parameter`。 |
| `cad-core/tests/test_p8_features.py` | 已新增三条 c6m4 focused tests，并保留 c5m10 known_gap guard。 |
| `C6M4-BLK-102` / `C6M4-SCOPE-102` / `C6M4-CAT-102` | 已更新为 `closed_S3`；`C6M4-ORC-101/102/103` 已更新为 `implemented_by_S3`。 |

## 边界

- c5m10 `part-sweep-located-profile-contract` expected 不修改，仍记录 FreeCADCmd wrapper build blocker。
- `part_sweep_located_profile_freecadcmd_wrapper_build_blocker` 不从 capability remaining gaps 删除；删除只能由 S5/S6 发布证据处理。
- S4 combined auxiliary + transition + tolerance 未实现；`C6M4-SCOPE-201` / `C6M4-BLK-201` 仍 pending。
- missing object target 由 graph 层先报 `missing_link_target`，仍带 object/target/subname；runtime invalid subname、non-vertex、multi-subname 使用 `SectionOptions[0].Location`。

## 验收

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- cad-core docs/CADCore6.0
for f in docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M4-PartWorkbenchSweepLocatedProfileCombinedPipeShellProductContract主线/工作步骤细分 --format markdown
```

Focused：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
```
