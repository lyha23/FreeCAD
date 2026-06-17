# P6 MakerHistory S0 声明口径与 live 基线复核

## 目标

冻结本主线的声明边界，复核当前 P6/P7/P8 文档、C ABI capabilities、focused tests 和源码之间的 live 状态。S0 不扫描大范围源码、不采 oracle、不写 C++。

## 输入

- `docs/CADCore方案/细化方案/00-CAD-Core完整抽取执行总览.md`
- `docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md`
- `docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md`
- `docs/CADCore方案/细化方案/11-P8-Part导入导出与Assembly后续.md`
- `cad-core/src/adapters/c_api/c_api.cpp`
- `cad-core/tests/test_adapters.py`
- `cad-core/tests/test_p6_topology.py`
- `cad-core/tests/test_p7_features.py`

## 声明口径

允许声明：

- “P6 MakerHistory 余量主线用于复核并收敛 ShapeFix / DressUp / transformed / taper 的 history 状态。”
- “当前文档与 capability 之间存在状态漂移，必须先以当前源码、tests 和 checked-in expected 为准复核。”
- “releaseGate 不是 backendGap；只有 FreeCAD authority 和 cad-core mismatch 同时存在，才进入 C++ 实现。”

禁止声明：

- “ShapeFix / DressUp / taper 全部未实现。”
- “capability 标为 covered 就等于 FreeCAD parity 完整发布。”
- “taper 当前一定还是 known_gap。”
- “可以用输出排序、几何类型猜测或 fixture 名称补齐 MakerHistory。”

## 纳入 / 排除

| 主题 | 状态 | 说明 |
| --- | --- | --- |
| ShapeFix_Root / ShapeBuild_ReShape history | 纳入 | 复核 modified / generated / deleted producer 是否已被 topo 消费 |
| DressUp AddSubShape / SupportTransform / chain history | 纳入 | 复核 Fillet / Chamfer / Draft / Thickness 与 transformed consumer |
| taper ThruSections history | 纳入 | 复核 Pad / Pocket / Part::Extrusion 是否已关闭 `known_gap:taper_history` |
| 文档 / capability 状态漂移 | 纳入 | 这是本主线首个 releaseGate |
| 完整 ShapeFix Python API | 排除 | 只迁移 cad-core 几何 / history 所需 producer |
| GUI、Workbench、TaskPanel、ViewProvider | 排除 | 不属于无状态 CAD Core |
| 跨请求 shape / BREP cache | 排除 | 仍保持 DocumentObject graph 为唯一持久源数据 |

## 状态词典

| 状态 | 进入条件 |
| --- | --- |
| `supported` | 当前源码、focused test / expected / capability 同时证明，且文档已对齐 |
| `releaseGate` | 源码和测试可能已覆盖，但文档 / capability / expected 或发布声明未对齐 |
| `notCollected` | 缺 FreeCAD oracle 或 expected，不允许直接写 C++ |
| `backendGap` | 有 FreeCAD authority、checked-in oracle 或 focused mismatch，且当前 cad-core 不匹配 |
| `unsupported` | FreeCAD 语义可见但本主线证据不足或需要单独产品决策 |
| `nonGoal` | 明确排除，并有用户 / 协议行为与 reopen 条件 |

## S0 live 复核结论

- live baseline：`pwd=/home/user/Chili3DProject/FreeCAD`，`HEAD=6d35327fcb`，`git log -1=6d35327fcb fix: 收敛 P7 transformed 拓扑 oracle`；工作区已有大量 pre-existing dirty / untracked 内容，S0 只回写本步骤文档和矩阵。
- 文档漂移：`09-P6-TopoNaming主路径.md` 和 `00-CAD-Core完整抽取执行总览.md` 原先仍把 taper `known_gap`、ShapeFix / DressUp / transformed 未覆盖写成硬缺口；S0 已改成 releaseGate / S3-S5 待裁决口径。
- capability / focused tests：`c_api.cpp` 当前暴露 `taper_history.status=covered_full` 且 `remaining_gaps=[]`；`producer_matrix.shape_fix.status=covered_no_generated_producer` 且 `remaining=[]`；`producer_matrix.dressup.status=done_first_slice` 且 `remaining=[]`；`producer_matrix.transformed.status=covered` 且 `remaining=[]`。`test_adapters.py` 对这些 remaining gaps 做了断言，`test_p6_topology.py` 和 `test_p7_features.py` 分别覆盖 taper mapper-backed history、ShapeFix modified/deleted history、DressUp / transformed slot 传播。
- P7/P8 文档：`10-P7-PartDesign常用生态.md` 已描述 DressUp / transformed 当前 covered 子集和复杂参数后续边界；`11-P8-Part导入导出与Assembly后续.md` 仍把 Assembly solver、完整 Link 账本等留作 P8 后续，不把 P6 MakerHistory releaseGate 误写成 backendGap。
- S0 状态：`P6MH-SCOPE-001` 保持 `releaseGate`，`P6MH-BLOCK-001` 指向 S0/S2 消歧，`P6MH-BG-001` 明确 releaseGate 不是 backendGap；S0 不把 releaseGate 提升为 supported / backendGap，具体裁决交给 S2-S5。

## 必须回写的矩阵行

- `P6MH-SCOPE-001`：live baseline drift。
- `P6MH-BLOCK-001`：文档 / capability / tests 状态消歧。
- `P6MH-BG-001`：releaseGate 分类。

## 验收标准

- `p6_maker_history_scope_review_matrix.tsv` 中 `P6MH-SCOPE-001` 保留明确 live baseline drift 结论。
- `p6_maker_history_blocker_queue.tsv` 中 `P6MH-BLOCK-001` 指向 S0/S2，不指向直接 C++。
- grep 不得在正式方案结论中出现未复核的 taper known-gap 必然结论或 ShapeFix 未实现必然结论。
- 执行：

```bash
rg -n "taper_history|shape_fix|dressup|transformed_pattern_full_history|remaining_gaps" cad-core/src/adapters/c_api/c_api.cpp cad-core/tests/test_adapters.py docs/CADCore方案/细化方案/09-P6-TopoNaming主路径.md docs/CADCore方案/细化方案/10-P7-PartDesign常用生态.md
for f in docs/FreeCAD几何生态迁移工程-细分/P6-MakerHistory-ShapeFix-DressUp-Taper收敛主线/矩阵/*.tsv; do
  awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"
done
git diff --check
```

## 非目标

- 不修改 C++。
- 不采集 FreeCAD expected。
- 不把 releaseGate 直接提升为 supported 或 backendGap。
