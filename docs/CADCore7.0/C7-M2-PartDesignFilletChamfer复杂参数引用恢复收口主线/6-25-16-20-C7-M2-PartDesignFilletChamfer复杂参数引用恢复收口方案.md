# C7-M2 PartDesign Fillet Chamfer 复杂参数引用恢复收口方案

## 背景

C7-M1 已把 Hole ModelThread 与标准孔表边界收口到 release gate，队列为空。P7 PartDesign 常用生态文档仍保留一条与当前 CAD Core 后端相关的 known gap：`Fillet / Chamfer 复杂参数组合、复杂引用变更后的完整稳定恢复`。基础 Fillet / Chamfer 并非空白能力：现有 fixtures/tests 已覆盖 Body-member native oracle、RefineModel、诊断型错误、SupportTransform 和链式 DressUp 被 transformed family 消费。

C7-M2 因此必须按“最小完整语义批次”推进：同一 FreeCAD 调用链、同一 DressUp Base / AddSubShape / SupportTransform 边界、同一 P7 expected 家族一起裁决。只有 S2 证明存在 active backend gap，S3 才能改 C++ 或 fixtures。

## 目标

- 冻结当前 Fillet / Chamfer live baseline，避免把已支持能力误判为新任务。
- 复核 FreeCAD `FeatureFillet.cpp`、`FeatureChamfer.cpp`、`FeatureDressUp.cpp` 的参数、选边、AddSubShape 和 SupportTransform 语义。
- 建立 complex parameter、multi-edge / UseAllEdges、Body/DressUp chain reference recovery、capability/docs publication 的矩阵。
- 裁决每个 row 的 route：already closed、oracle pending、backend gap、publication-only、non-goal。
- 对 S2 批准的 backend gap 实施正式分层修复；如果没有 backend gap，则做 no-code publication closure。

## 范围

### 必须纳入同一批次

- Fillet `Radius`、`UseAllEdges`、selected edge、continuous edge expansion、multi-edge。
- Chamfer `ChamferType`、`Size`、`Size2`、`Angle`、`FlipDirection`、`UseAllEdges`。
- DressUp Base LinkSub、Body cumulative shape、前序 Fillet/Chamfer Base、stable subname 输入和 diagnostic。
- `SupportTransform=true` 的 AddSubShape cache、slot ownership 和链式 DressUp source base。
- `cad-core/fixtures/p7` 现有 Fillet / Chamfer / SupportTransform fixtures 与 focused tests。

### 明确排除

- GUI Fillet / Chamfer task panel、交互选择器和 preview UI。
- Draft、Thickness 等非 Fillet / Chamfer dress-up 类型。
- full topo naming / full MapperHistory 泛化工程。
- 在输出端靠 fixture 名称、边顺序、source edge 猜测稳定引用。
- transformed family 超出 Fillet / Chamfer SupportTransform 链路的复杂参数全集。

## 步骤

### S0 live baseline

记录 `pwd`、`HEAD`、`git status`、C7-M1 队列为空、P7 文档 known gap、现有 fixture/test/capability 覆盖。S0 只改 docs/matrices，不改 C++、fixtures、expected 或 tests。

### S1 FreeCAD 源码与 oracle 候选

阅读 FreeCAD 源码并更新 `source_candidates`、`input_contract`、`oracle_fixture` 矩阵。必须引用具体文件、类/函数和关键字段名，不能只写“参考 FreeCAD”。

### S2 准入裁决

把 rows 路由为：

- `already_closed_expected_backed`
- `oracle_pending_collect`
- `backend_gap_requires_implementation`
- `publication_closure_only`
- `diagnostic_non_goal`

如果没有 `backend_gap_requires_implementation`，S3/S4 只能做 no-code publication closure。

### S3 实现或 diagnostic 边界收口

只有 S2 授权时才改 C++。涉及 FreeCAD 语义的 public API、executor 主路径、mapper/history 字段必须在相邻注释写明 FreeCAD 源文件、类/函数和关键短句。若需要引用恢复，优先补 `topo` / history / naming 正式能力，不允许在 adapter 或 executor 输出端修剪。

### S4 fixtures/tests/capability 发布

把 S2/S3 route 同步到 fixtures、expected、focused tests、capability docs 和本包 README。expected 必须来自 FreeCAD oracle 或明确 diagnostic，不得从当前 `cad-core` 输出倒推。

### S5 release gate

清空队列，按代码变更实际范围运行 focused build/tests。仅当 C++、fixtures、expected、adapter schema 或 topo/history 广泛变动时才提升到重型阶段回归。

## 验收分层

### 本轮文档短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M2-PartDesignFilletChamfer复杂参数引用恢复收口主线
git diff --check
```

### 实现短跑

S3 若改 C++，先由 worker 从当前 `cad-core/tests/test_p7_features.py` 读取真实 test names，再选择 Fillet / Chamfer / DressUp / SupportTransform focused filters。不要在方案里手写过期 test name 当硬性命令。

### 阶段回归

S5 若触发代码或 expected 发布变更，默认至少运行：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest
```

## 收口标准

- S2 每个 row 都有 route、证据和下一步。
- active backend gap 不被标成 supported，unsupported/non-goal 不被写入 capability supported。
- fixtures/expected/tests/capability/docs 口径一致。
- 没有 fixture-specific 输出修正或 executor 端引用猜测。
- C7-M2 `工作步骤细分` 队列为空后，才允许声明本包完成。
