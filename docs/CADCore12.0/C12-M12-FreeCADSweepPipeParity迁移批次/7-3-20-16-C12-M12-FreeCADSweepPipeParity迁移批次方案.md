# C12-M12 FreeCAD Sweep / Pipe Parity 迁移批次方案

## 目标

把 FreeCAD 的 Sweep / Pipe 行为迁移到 `cad-core`，让用户样例不再依赖非 FreeCAD 修补路径。迁移完成后，`Part::Sweep` 与 `PartDesign::AdditivePipe` / `SubtractivePipe` 应在以下方面可被 focused tests 和 expected 证明：

- profile / spine / section link 解析与 FreeCAD 一致。
- `Mode`、`Transition`、auxiliary spine、binormal、support、tolerance、law 选项按 FreeCAD 调用顺序进入 `BRepOffsetAPI_MakePipeShell`。
- PartDesign solid 化、front/back cap、sewing、Body Tip 与 add/cut Boolean history 与 FreeCAD source 对齐。
- Part Workbench `Sweep` wrapper 与 advanced helper DTO 保持 source-backed diagnostics。
- mesh response 没有零渲染法线，但 mesh 仅作为 response gate，不替代 BRep parity。

## 迁移切分

### S0 live 基线

冻结当前 `HEAD`、dirty boundary、已有 sweep/pipe fixtures、focused tests 与 capability wording。若工作区已有未提交 `cad-core` 改动，必须先判定是否属于本包，非本包改动不得被顺手覆盖。

### S1 FreeCAD source authority

逐段复核：

- `Pipe::execute()`：profile、spine、multi-section、solid/cap/sewing、Body feature flow。
- `Pipe::setupAlgorithm()`：Fixed/Frenet/Auxiliary/Binormal mode 与 transition。
- `TopoShape::makeElementPipeShell()`：Part Sweep shared shell builder 与 history。
- `Sweep::execute()`：Part Workbench wrapper。
- `BRepOffsetAPI_MakePipeShellPyImp.cpp`：advanced public helper。

S1 只写 source matrix，不改代码。

### S2 cad-core drift audit

对比当前 `cad-core`：

- `part_design/feature_pipe.cpp` 是否出现 FreeCAD source 没有的 profile relocation、invalid face rebuild、silent fallback。
- `part/topo_shape_expansion.cpp` 是否已经有 correct `SetMode` / `SetTransitionMode` / `Add` / `SetLaw` / `Build` / `MakeSolid` / `Simulate` / sewing 顺序。
- `part/part_sweep.cpp` 是否把 advanced DTO 与 Part Sweep wrapper 混入 PartDesign Pipe。
- tests/expected 是否只覆盖旧产品契约，缺少用户失败样例。

S2 输出 drift rows 与 code landing，不直接修。

### S3 oracle fixture 与红灯闭环

把用户失败样例和最小代表样例变成 native/current 对照：

- 用户失败样例：从实际 input/output 中提取最小 profile/spine/pipe request。
- Standard vs Frenet：验证 Standard 不被误改成 Frenet。
- solid cap/sewing：验证端面、sewing history、solid status。
- multi-section / auxiliary / binormal / support：验证 helper mode 与 diagnostics。
- Part Workbench Sweep：验证 wrapper 与 PartDesign Pipe 不混线。

只有当 native expected 稳定、current mismatch 可复现、failure 定位到 `cad-core` 实现时，S4/S5 才打开代码 gate。

### S4 PartDesign Pipe 主路径迁移

最小实现目标：

- 按 FreeCAD source 重写或收紧 PartDesign Pipe executor 的 profile/spine/section 解析。
- 删除或隔离非 FreeCAD 行为：自动 profile-to-spine-start 吸附、invalid planar side rebuild、mesh-level 几何修正。
- 复刻 `setupAlgorithm()` 的 mode/transition/auxiliary/binormal/law 调用顺序。
- 复刻 PartDesign open shell cap/sewing/solidification 与 Body add/cut history。
- 补 focused tests 与 expected。

### S5 Part Sweep wrapper 与 response 收口

最小实现目标：

- 保证 `Part::Sweep` 仍走 shared `makeElementPipeShell`，但 wrapper diagnostics、advanced helper DTO 与 PartDesign Pipe 分层。
- response 中记录 source-backed history、mode、transition、solid、sections、spine、advanced options。
- mesh `edgeSegments` / normals 作为 response quality gate，不能倒推 BRep。
- 确认 frontend 所需字段由后端稳定给出，若前端仍丢失则分流到 `my-chili3d` consumer sync。

### S6 发布闸门

发布条件：

- S1 source matrix 全部 reviewed。
- S2 drift rows 对每个差异有 owner step 与 close condition。
- S3 至少有一个用户失败样例或代表样例的 red-to-green evidence。
- S4/S5 focused tests 与 expected 通过。
- root README、package README、矩阵、capability / adapter wording 已同步。

## 代码落点候选

```text
cad-core/src/part_design/feature_pipe.cpp
cad-core/src/part/topo_shape_expansion.cpp
cad-core/src/part/part_sweep.cpp
cad-core/src/runtime/feature_registry.cpp
cad-core/tests/test_p7_features.py
cad-core/tests/test_p8_features.py
cad-core/fixtures/c12m12/
```

是否新增 `c12m12` fixture 目录由 S3 决定；如果现有 c5/c6 fixtures 足够表达用户 failure，就优先复用现有目录并补 expected。

## 最小验证命令

开包验证：

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/矩阵/*.tsv
```

实现后 focused 验证由 S4/S5 固化，候选命令：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests.test_p7_features.CadCoreP7FeaturesTest.test_c12m12_partdesign_pipe_freecad_standard_user_regression
python3 -m unittest tests.test_p7_features.CadCoreP7FeaturesTest.test_c12m12_partdesign_pipe_freecad_cap_sewing_history
python3 -m unittest tests.test_p8_features.CadCoreP8FeaturesTest.test_c12m12_part_sweep_make_element_pipe_shell_parity
python3 -m unittest tests.test_p8_features.CadCoreP8FeaturesTest.test_c12m12_part_sweep_advanced_helper_response_contract
```

## 非目标

- 不把 `chili3d/docs/sweep-implementation.md` 当成 FreeCAD parity spec。
- 不用 mesh 修复替代 `BRepOffsetAPI_MakePipeShell` 参数和 history 修复。
- 不在没有 native/current mismatch 时修改 shared shape exporter。
- 不把 Part Workbench advanced helper 的 product contract 强塞到 PartDesign Pipe。
- 不绕过 Body replay / Tip / add-cut history。
