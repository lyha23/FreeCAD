# C6-M5-S6 阶段回归与 release-gate

## 目标

对 C6-M5 的 Filling product contract 做阶段回归和 heavy 收口。S6 通过后，才能把主线写成已发布，并更新 `docs/CADCore6.0/README.md` 的状态。

## 下一轮代码落点

如果 S5 后仍有 `backendGap` 或 implementable `unsupported`，S6 不能只做文档收尾，必须把剩余代码落点写清：

| blocker / scope | C++ 落点 | FreeCAD source authority | focused tests | 成功标准 |
| --- | --- | --- | --- | --- |
| 已实现但待 release gate 的 `BLK-101` 到 `BLK-202` | `cad-core/src/part/part_filling.cpp`、`cad-core/src/part/topo_shape_expansion.cpp`、`cad-core/src/runtime/capability_contract.cpp` | `AppPartPy.cpp::makeFilledFace()`、`TopoShapeExpansion.cpp::makeElementFilledFace()`、`BRepOffsetAPI_MakeFillingPyImp.cpp` | `cad-core/tests/test_p8_features.py` Filling tests | 对应 `remaining_gaps` 只在 fixture、test、capability 和阶段闸门同步通过后删除。 |

## 发布闸门

- `cmake --build build` 通过。
- P8 focused tests 通过。
- expected fixture / adapter focused tests 通过。
- heavy 收口只记录最终结论。
- `step_goal_queue.py` 对 C6-M5 工作步骤目录不再返回待执行实现步骤。

## 禁止捷径

- 不用 fixture 名称分支。
- 不用几何类型猜测、bbox/面积排序、输出修剪关闭 gap。
- 不把 adapter 层作为业务语义落点。
- 不把 persistent BREP 或 wrapper state 加回产品合同。

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
python3 -m unittest tests.test_p6_topology tests.test_p8_features tests.test_expected_fixtures tests.test_adapters
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线/工作步骤细分 --format markdown
git diff --check -- cad-core docs/CADCore6.0/C6-M5-PartWorkbenchFillingSurfaceSupportOrderParamProductContract主线 docs/CADCore6.0/README.md
```

验收通过后，将本文重命名为 `6-24-16-26-【已实现】C6-M5-S6-阶段回归与release-gate.md`，并把本主线 README / 根 README 状态更新为已发布。若重型收口因环境问题失败，必须在矩阵中写清是否为环境 / OCCT gap，不能直接标发布通过。

## 非目标

- 不跑全量上游 FreeCAD build。
- 不借 S6 扩大到 C6-M6 候选。
- 不声明 FreeCAD parity。
