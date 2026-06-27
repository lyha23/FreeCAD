# C9-M3 Assembly DistanceType default boundary 批次方案

## 定位

C9-M3 是 C9-M2 之后的新 Assembly DistanceType 子线。C9-M2 已经关闭 request-local marker、bundled `offsetPlc`、placement writeback 和 zero Angle fallback；C9-M3 不在这些已关闭能力上继续补丁，而是处理 capability 里仍明确保留的 DistanceType diagnostic/default 边界。

## 最小完整语义批次

本批次不只做 `PointCurve` 单 case，也不只做 `PlaneCone` 单 fixture。完整边界是：

- `PointCurve`：FreeCAD 明确映射为 `ASMTPointInPlaneJoint` + `offset`，当前 cad-core DTO 已有 solver class，但 runtime 仍有 product acceptance guard。
- 已有 default expected：`PlaneCone`、`LineCylinder`、`CurvePlane`、`Other` 已有 native expected 和 current diagnostic tests。
- 同源 default branch 扩面：`CylinderCone`、`ConeCone`、`ConeTorus`、`ConeSphere`、`PointCone`、`PointTorus`、`LineSphere`、`LineCone`、`LineTorus`、`CurveCylinder`、`CurveSphere`、`CurveCone`、`CurveTorus` 共享 FreeCAD default planar branch，应在 S4 统一判定是采 oracle 后支持、继续 non-goal，还是拆出更小的产品边界。

## 分步策略

| 步骤 | 决策点 | 输出 |
| --- | --- | --- |
| S0 | C9-M2 是否已闭环，当前 capability 仍暴露哪些 DistanceType diagnostic/default 边界 | baseline、claim、forbidden claims。 |
| S1 | FreeCAD source 与 current cad-core 覆盖是否足够定义语义 | source candidates、expected inventory、current guard。 |
| S2 | 哪些 scope 可实施、哪些必须先采 oracle、哪些保持 non-goal | scope / blocker / backend gap 初始路由。 |
| S3 | `PointCurve` 是否可从 diagnostic 转 expected-backed supported | existing expected、current mismatch、tests 路由。 |
| S4 | default planar branch 是否按同一 FreeCAD default 语义批量支持 | 既有 expected 激活、缺 expected 采集、批量 backendGap 判定。 |
| S5 | capability 与 diagnostics 是否能诚实发布 | capability tests、unsupported diagnostics guard、non-goal 保留。 |
| S6 | 对 expected-backed mismatch 落 C++，或关闭 release gate | runtime implementation、fixtures/tests/capability/docs 收口。 |

## 关键取舍

- `PointCurve` 是优先实现候选，因为 FreeCAD 源码和 checked-in expected 已经足够明确，且 cad-core 主要阻塞点是 explicit diagnostic guard。
- default branch 不能靠“所有未映射都自动支持”一笔带过。S4 必须确认每个 DistanceType 的 marker、JCS swap、native expected 和 current solver DTO；缺 expected 的组合保持 `notCollected`，不得直接写 supported。
- `non_assembly_link_subshape_primitive_frame_generalization` 不进 C9-M3。它需要新的 request-local primitive-frame DTO 和产品边界，不是 DistanceType default branch 的自然延伸。
- GUI/session、persistent solver state、cross-request placement cache 继续作为 non-goal。

## S6 代码落点预期

若 S3/S4 形成 expected-backed backend gap，S6 允许修改：

- `cad-core/src/assembly/joint_solver.cpp`：移除 `PointCurve` diagnostic guard；为 accepted default branch 设置 `ASMTPlanarJoint` + `offset`，并保持 unsupported diagnostics 对未采集 / 未接受 case 可见。
- `cad-core/tools/collect_freecad_expected.py`：更新 accepted expected metadata，保留 native oracle 字段，不从 current output 改 expected。
- `cad-core/tests/test_p8_features.py`：把 accepted fixtures 从 diagnostic/default boundary assertions 改为 current parity assertions。
- `cad-core/tests/test_adapters.py`：更新 `distance_type_extended_geometry` supported / non-goals / default boundaries。
- `cad-core/src/runtime/capability_contract.cpp`：同步 capability publication。
- `cad-core/fixtures/c3m6/*`：新增缺失 default branch fixtures 和 FreeCAD expected。

禁止路径：fixture 名称分支、bbox / area / angle 猜测、输出排序修补、adapter 层字符串改写隐藏 runtime 缺口、跨请求 solver session 或 BREP / placement cache。

## 验收分层

文档短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore9.0/C9-M3-AssemblyDistanceTypeDefaultBoundary批次 docs/CADCore9.0/README.md
git diff --check
```

实现闸门：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_adapters
./cad-core capabilities > /tmp/c9m3-capabilities.json
```

native oracle 刷新仅在 S3/S4 需要采集或更新 expected 时执行；必须记录 FreeCADCmd 版本和 collection-time expected 基线。
