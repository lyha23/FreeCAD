# C8-M4 Part GeomPlate CurveConstraint Criteria Request-Local 批量收口主线总入口

## 主线目标

C8-M4 的目标是收口 `Part.GeomPlate` 中 CurveConstraint criteria 的 request-local 后端缺口：`G0Criterion`、`G1Criterion`、`G2Criterion` 必须作为同一 DTO / OCCT setter 边界批量裁决，不能按单个字段或单个 fixture 继续薄推进。

本包只在证据允许时把 `cad-core` 的请求内 CurveConstraint criteria 从 `unsupported_curve_criteria` 诊断边界转为 product contract。若 FreeCAD 原生 Python setter 仍抛 `NotImplementedError`，该 native setter 仍保留为 native blocker / non-parity 说明，不得宣称 FreeCAD setter parity 已支持。

## 当前基线

- `pwd=/home/user/Chili3DProject/FreeCAD`
- S0 live HEAD：`7a9fa7bdd3`（`7a9fa7bdd3 docs: 创建 C8-M4 GeomPlate criteria 收口方案`），`git status --short -uall` 无输出。
- 方案创建旧起点：`48900289ec`（`48900289ec chore: 完成 C8-M3 S6 capability 发布闸门`），已被 S0 live baseline 覆盖为执行证据。
- C8-M3 队列为空，`part_workbench.conic_curves.remaining_gaps=[]`。
- C8-M4 S0 执行前队列首项是 S0；S0 完成后队列首项前移到 S1。
- S1 live HEAD：`d3d02af5e3`（`d3d02af5e3 docs: 完成 C8-M4 S0 live 基线冻结`），开始工作区干净。
- S1 已完成 source/current coverage 复核并关闭 `C8M4-BLOCKER-101`：FreeCAD CurveConstraint native setters 仍 `NotImplemented`；getter 证明 OCCT criteria read state；PointConstraint setter 仅 analog；当前 Curve DTO 没有 G0/G1/G2 字段，parser/apply path 尚未支持 Curve criteria。
- 当前 known gap 输入：`part_workbench.geomplate.narrowed_gaps.curve_constraint_criteria_setters_not_implemented`。
- 当前 diagnostic 输入：`unsupported_curve_criteria`。
- S0 禁止声明：不采 FreeCAD oracle，不新增 fixture / expected / tests / collector，不修改 C++ / Rust / FreeCAD `src/`，不删除上述 gap / diagnostic，不声明 FreeCAD native `CurveConstraintPy` setter parity 已支持。

## 为什么同轮批量处理

`CurveConstraintPyImp.cpp` 的 `setG0Criterion` / `setG1Criterion` / `setG2Criterion` 是同一 FreeCAD wrapper 边界；`cad-core` 侧若要打开 request-local support，也必须同时补齐 `GeomPlateCurveConstraintSource`、`readCurveConstraints()` 和 `addCurveConstraint()` 这一整条 DTO / parser / builder 链路。若只处理 G0 或只删 diagnostic，不同时覆盖 G1 / G2、fixtures、tests 和 capability，会留下继续单 fixture 补洞的状态。

因此 C8-M4 把以下代表场景纳入同一轮：

- FreeCAD native `CurveConstraintPy` setter 是否仍为 `NotImplemented`。
- `cad-core` DTO 是否已经可表达三项 criteria，或仍缺 Curve criteria 字段。
- `cad-core` parser 当前为何阻断 criteria 字段。
- OCCT `GeomPlate_CurveConstraint::SetG0Criterion` / `SetG1Criterion` / `SetG2Criterion` 是否已经在 Curve 构造路径落地，或仍只存在于 Point analog。
- fixtures / focused tests / capability 是否能把三项 criteria 一次性发布。

## 证明链条

```text
S0 live 基线与批量范围冻结
  -> S1 FreeCAD source / current cad-core coverage 复核（已完成）
  -> S2 scope / blocker / non-goal / implementation gate 分类
  -> S3 FreeCAD CurveConstraint criteria 原生 setter 边界复核
  -> S4 cad-core request-local criteria fixture / DTO / parser 准入
  -> S5 capability 与 non-goal 重分类准入
  -> S6 实现与发布闸门
```

## FreeCAD 调用依据

| 语义 | FreeCAD 源码入口 | 关键行为 |
| --- | --- | --- |
| CurveConstraint 构造 | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::CurveConstraintPy::PyInit()` | 构造 `GeomPlate_CurveConstraint` |
| CurveConstraint criteria setter | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion` | 当前需复核是否仍为 `NotImplementedError` |
| CurveConstraint criteria getter | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/CurveConstraintPyImp.cpp::getG0Criterion/getG1Criterion/getG2Criterion` | 读取 OCCT criteria |
| PointConstraint criteria analog | `/home/user/Chili3DProject/FreeCAD/src/Mod/Part/App/GeomPlate/PointConstraintPyImp.cpp::setG0Criterion/setG1Criterion/setG2Criterion` | 已实现 setter，可作为同类 criteria 行为参考，不等于 Curve setter parity |

## cad-core 落点

| 层 | 当前代码落点 | 职责 |
| --- | --- | --- |
| DTO | `/home/user/Chili3DProject/FreeCAD/cad-core/include/cad_core/part/part_geomplate.h::GeomPlateCurveConstraintSource` | S1 复核确认当前没有 G0/G1/G2 字段；字段只在 PointConstraint source 与 source evidence 中存在 |
| parser / diagnostics | `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/part_geomplate.cpp::readCurveConstraints()` | 当前发现 criteria 字段后发布 `unsupported_curve_criteria`，并在创建 Curve source 前返回 |
| OCCT apply | `/home/user/Chili3DProject/FreeCAD/cad-core/src/part/part_geomplate.cpp::addCurveConstraint()` | S1 复核确认当前 Curve path 未调用 `SetG*Criterion`；`SetG*Criterion` 调用只在 PointConstraint path |
| capability | `/home/user/Chili3DProject/FreeCAD/cad-core/src/runtime/capability_contract.cpp` | 发布 `part_workbench.geomplate` 支持状态、narrowed gaps 和 diagnostics |
| tests | `/home/user/Chili3DProject/FreeCAD/cad-core/tests/test_p8_features.py`、`test_adapters.py`、`test_diagnostics.py` | focused validation 和 capability smoke |

## 产物索引

| 类型 | 路径 | 用途 |
| --- | --- | --- |
| README | `README.md` | 包入口 |
| 方案 | `6-27-02-33-C8-M4-PartGeomPlateCurveConstraintCriteriaRequestLocal批量收口方案.md` | 实施方案 |
| 工作步骤总入口 | `工作步骤细分/6-27-02-33-【已实现】C8-M4工作步骤总入口.md` | 队列索引 |
| S0 | `工作步骤细分/6-27-02-34-【已实现】C8-M4-S0-live基线与批量范围冻结.md` | 声明与基线 |
| S1 | `工作步骤细分/6-27-02-35-【已实现】C8-M4-S1-FreeCAD源码与current覆盖批量复核.md` | source authority |
| S2 | `工作步骤细分/6-27-02-36-C8-M4-S2-scope准入与blocker矩阵.md` | route 分类 |
| S3 | `工作步骤细分/6-27-02-37-C8-M4-S3-FreeCADCurveConstraintCriteria原生边界复核.md` | native setter 边界 |
| S4 | `工作步骤细分/6-27-02-38-C8-M4-S4-cad-core请求内criteria落点与fixture准入.md` | cad-core DTO / fixture gate |
| S5 | `工作步骤细分/6-27-02-39-C8-M4-S5-capability与non-goal重分类准入.md` | capability / non-goal |
| S6 | `工作步骤细分/6-27-02-40-C8-M4-S6-实现与发布闸门.md` | implementation / release gate |
| source candidates | `矩阵/c8m4_geomplate_criteria_source_candidates.tsv` | FreeCAD / cad-core source |
| scope review | `矩阵/c8m4_geomplate_criteria_scope_review_matrix.tsv` | scope / status |
| oracle plan | `矩阵/c8m4_geomplate_criteria_oracle_plan.tsv` | native / request-local expected plan |
| backend gap | `矩阵/c8m4_geomplate_criteria_backend_gap_classification.tsv` | implementation gate |
| blocker queue | `矩阵/c8m4_geomplate_criteria_blocker_queue.tsv` | blocker / close condition |
| non-goal | `矩阵/c8m4_geomplate_criteria_non_goal_registry.tsv` | native blocked / GUI / wrapper exclusions |
| validation | `矩阵/c8m4_geomplate_criteria_validation_matrix.tsv` | 验收命令 |

S0 已完成 live baseline 与批量范围冻结；S1 已完成 source/current coverage 复核；S2-S6 仍待执行。执行后续步骤时必须继续从 live baseline 开始，不继承旧结论。
