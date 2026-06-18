# P8 AssemblySolver S7 NativePlacementParity backendGap 修复

## 结论

S7 已实现。`P8ASM-SCOPE-006` 从 `backendGap` 转为 `supported`：9 个 `cad-core/fixtures/c3m6/expected/*.freecad.json` 已全部移除 `known_gap`，`CadCoreExpectedFixtureTest` 不再跳过 native solver placement parity。

## 当前基线

- cad-core 保持 hard-linked real OndselSolver；未恢复 `representative_ondsel_solver` 或 `CAD_CORE_ENABLE_ONDSEL_SOLVER`。
- Assembly solver 仍是 request-local adapter：`documentObjectUpdates.action=assembly_set_placement` 是前端 graph 更新建议，不是后端持久状态。
- Assembly display compound 在本次请求内消费 solver placement writeback 后再汇总 bbox / topology，匹配 FreeCAD `AssemblyObject::setNewPlacements()` 后的 native oracle。
- object-level JCS placement、Distance / multi-component writeback、invalid grounded 和 ungrounded 9 个 native expected 均已验收。

## 代码落点

| 子项 | 结果 |
| --- | --- |
| Solver DTO | `AssemblySolveRequest` / `JointConstraint` 保留 solver-side writeback normalization 所需字段 |
| real-only adapter | 代表 fallback 已删除；无 GroundedJoint 按 native observed solved/no updates 处理 |
| Distance writeback | object-level Distance 对齐 ComponentB `[4,0,2]` 与多组件 `[4,0,2]` / `[8,0,4]` |
| invalid grounded | 对齐 native expected：两个 grounded components 写回共享 rotation，不再走 drag-only validation gate |
| Assembly display | `AssemblyObject` 汇总 shape 时应用本次 solver placement updates，避免 bbox 使用 stale child placement |

## 验收

本轮已通过：

```bash
cd cad-core
cmake --build build --target cad-core cad_core_ffi
python3 -m unittest tests.test_p8_features.CadCoreP8FeatureTest
python3 -m unittest tests.test_expected_fixtures.CadCoreExpectedFixtureTest
```

本轮也用 `FreeCADCmd` 对 `assembly-joint-group-diagnostics` 和 `assembly-grounded-distance-joint-real-solver` 做过 native bbox spot-check，确认 fixed-joint p8 expected 需要 post-solve bbox `[0,0,0] -> [2,2,2]`，Distance c3m6 expected 为 `[0,0,0] -> [6,2,4]`。

## 剩余边界

- 复杂 Distance geometry、额外 JointType、完整 Joint placement / constraint 仍按 `P8ASM-SCOPE-007` 的 unsupported / notCollected 队列处理。
- GUI drag / postDrag、跨请求 solver session、完整 Assembly transaction lifecycle 仍是 `P8ASM-SCOPE-009` nonGoal。
- 完整 FreeCAD Link 账本、持久写回事务和 Worker / WASM / Web 产品化 adapter 不属于 S7。
