# P6 MakerHistory S6 Oracle 实现与发布闸门

## 目标

消费 S2-S5 留下的 `notCollected`、`backendGap`、`unsupported` 和 `releaseGate`。S6 必须区分两类结果：已经实现但文档 / capability 未发布的 releaseGate，以及确有 FreeCAD mismatch 的 C++ backendGap。

## 执行结论

2026-06-17 已完成 S6 发布闸门：S3-S5 没有产生 C++ `backendGap`，因此本轮不写 C++、不刷新 expected。S6 已把 P3b、P6 和总览文档从旧 `history_partial:taper` / `known_gap:taper_history` / releaseGate 口径回写到当前 supported 结论，并把 `P6MH-SCOPE-001`、`P6MH-SCOPE-006` 关闭为 `supported`。

当前矩阵剩余：`P6MH-SCOPE-005 = notCollected`，表示 ShapeFix / DressUp / taper 造成的更复杂 split / deleted 旧引用恢复还没有具体 FreeCAD oracle 场景；它不是 backendGap，也不触发本轮 C++。`P6MH-BLOCK-005` 继续作为后续 oracle 队列入口保留。

## 输入

- `p6_maker_history_scope_review_matrix.tsv`
- `p6_maker_history_blocker_queue.tsv`
- `p6_maker_history_backend_gap_classification.tsv`
- S3 ShapeFix 复审结论
- S4 DressUp / Refine 传播复审结论
- S5 taper 复审结论

## 实施顺序

1. 先跑 S0-S5 的 TSV 和 grep 验收，确认没有 stale status。
2. 对 `releaseGate`：回写正式 P6/P7/总览文档、C ABI capabilities 或 focused tests，使状态一致。
3. 对 `notCollected`：补 FreeCAD expected / focused oracle；仍不能采集时保留 notCollected 或转 nonGoal，不直接写 C++。
4. 对 `backendGap`：按 FreeCAD 调用链补 `topo` / `part` / `part_design` C++，再补 focused tests。
5. 对 `unsupported`：只有 request-local 语义清楚且用户要修时才转 C++，否则保持 diagnostic。
6. 最后运行发布闸门，更新矩阵状态，不批量标 `【已实现】` 未满足验收的步骤。

## 下一轮代码落点

| blocker / scope | 触发条件 | C++ 落点 | FreeCAD authority | focused tests | 成功标准 |
| --- | --- | --- | --- | --- | --- |
| `P6MH-BLOCK-002` / `P6MH-SCOPE-002` | S3 证明 ShapeFix generated / deleted / modified 与 FreeCAD mismatch | `cad-core/src/part/shape_fix.cpp`; `cad-core/src/part/topo_shape.cpp`; `cad-core/include/cad_core/part/shape_fix.h` | `TopoShape.h::MapperHistory(ShapeFix_Root&)`; `AppPartPy.cpp::ShapeFixModule` | `cad-core/tests/test_adapters.py`; 新增 focused ShapeFix fixture/test | ShapeFix producer_matrix 无虚假 gap；NamedShape history status 能解释 modified / deleted 或明确 no-generated |
| `P6MH-BLOCK-003` / `P6MH-SCOPE-003` | S4 证明 DressUp / Refine / transformed 传播缺 history | `cad-core/src/part_design/feature_dress_up.cpp`; `cad-core/src/part_design/feature_dress_up_support.h`; `cad-core/src/part/topo_shape.cpp` | `FeatureDressUp.cpp::DressUp::getAddSubShape()`; `FeatureTransformed.cpp` | `cad-core/tests/test_p7_features.py`; `cad-core/tests/test_adapters.py` | AddSubShape slot、SupportTransform、chain DressUp、terminal split/deleted/merge 可追溯 |
| `P6MH-BLOCK-004` / `P6MH-SCOPE-004` | S5 证明 taper 仍有 `known_gap:taper_history` 或 ThruSections relation 缺失 | `cad-core/src/part/extrusion_helper.cpp`; `cad-core/src/part/topo_shape.cpp`; `cad-core/src/part_design/feature_extrude.cpp`; `cad-core/src/part/part_extrusion.cpp` | `ExtrusionHelper.cpp`; `TopoShapeExpansion.cpp::MapperThruSections` | `cad-core/tests/test_p6_topology.py`; `cad-core/tests/test_feature_flows.py`; `cad-core/tests/test_adapters.py` | Pad / Pocket / Part::Extrusion taper 不再暴露 stale known_gap，history relation 有 focused regression |
| `P6MH-BLOCK-005` / `P6MH-SCOPE-005` | S3-S5 采到复杂 split / deleted oracle mismatch | `cad-core/src/part/topo_shape.cpp`; `cad-core/src/runtime/recompute.cpp` | `PropertyTopoShape.cpp`; `TopoShapeMapper.cpp`; `PropertyLinks.cpp` | 新增 P6 stable subname recovery focused test | 同类唯一恢复写 ElementMap；一对多 split 只输出 diagnostic |

## 禁止路径

- 禁止按 fixture 名称分支。
- 禁止按几何类型、面积、长度、bbox、输出顺序猜 source ownership。
- 禁止在 adapter 层写业务语义。
- 禁止把 BREP、shape、NamedShape 或 ElementMap 作为跨请求持久状态。
- 禁止仅修改 expected 或放宽断言来关闭 backendGap。

## 验证分层

本轮短跑：

```bash
git diff --check
for f in docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

代码落地后 focused 验证：

```bash
python3 -m unittest tests.test_p6_topology.CadCoreP6TopologyTest.test_p6_taper_thru_sections_history_is_mapper_backed
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

阶段回归条件：

- 改 `topo_shape.cpp` 的通用 history helper。
- 改 `runtime` resolver。
- 改 expected collector 或 native oracle。

阶段回归命令：

```bash
cmake --build build
python3 -m unittest tests/test_expected_fixtures.py
```

## 验收标准

- 每个 `P6MH-BLOCK-*` 都关闭为 `supported`、保留为 `notCollected`、转 `backendGap` 并有 C++ 落点，或转 `nonGoal` 并有 reopen 条件。
- 正式 P6/P7/总览文档不再与 C ABI capabilities 对同一能力给出相反状态。
- 若有 C++ 改动，必须有 focused test 或 expected fixture 约束。
- `git diff --check` 和 TSV field-count 检查通过。

## 非目标

- 不一次性实现完整 MapperHistory 生命周期所有 maker。
- 不迁移完整 ShapeFix Python API。
- 不实现完整 Assembly / Link 持久事务。
