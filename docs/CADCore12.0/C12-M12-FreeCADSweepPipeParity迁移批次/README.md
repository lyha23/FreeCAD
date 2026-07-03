# C12-M12 FreeCAD Sweep / Pipe Parity 迁移批次

C12-M12 是用户点名打开的 FreeCAD Sweep / Pipe 迁移方案包，目标是把 FreeCAD 原生 `Part::Sweep` 与 `PartDesign::AdditivePipe` / `PartDesign::SubtractivePipe` 的语义迁移到 `/Users/li/Chili3DProject/FreeCAD/cad-core`。

本包不是沿用旧 `chili3d` sweep 实现，也不把前端预览、mesh 法线修补或 profile 自动吸附当成 FreeCAD parity。所有实现必须先以 FreeCAD source authority 和 current `cad-core` drift audit 为依据，再进入 oracle fixture 与最小代码迁移。

## 当前基线

- 创建基线：`pwd=/Users/li/Chili3DProject/FreeCAD`。
- 创建基线：`HEAD=3c5ccff1fe`（`3c5ccff1fe feat: 补齐 PartDesign 开放轮廓与 Thicken 语义`）。
- 创建时 `git status --short --untracked-files=all` 对 `cad-core` 与 `docs/CADCore12.0` 无既有改动输出；本包只新增 `docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/` 并更新 `docs/CADCore12.0/README.md`。
- C12-M12 是用户单独要求的 implementation migration 方案包；它不继承 C12-M10 CopyOnChange pending 语义，也不重开 C12-M11 closed internal edge backend contract。
- 工作步骤总入口已关闭：`工作步骤细分/7-3-20-17-【已实现】C12-M12工作步骤总入口.md` 已确认包结构、S0-S6 队列顺序和 TSV 字段数；后续队列从 S0 `live 基线与 dirty 边界冻结` 继续。

## 问题定义

当前要解决的是 FreeCAD sweep 语义没有被完整复刻，而不是单纯的 mesh 显示修补。典型风险包括：

1. `PartDesign::Pipe` 的 `Mode` / `Transition` / auxiliary spine / binormal / support / law 语义被简化成固定 `BRepOffsetAPI_MakePipeShell` 调用。
2. `Part::Sweep` 与 `PartDesign::Pipe` 的 solid 化路径、cap/sewing history、profile placement、multi-section 行为被混用。
3. 后端加入非 FreeCAD 行为，例如 profile 到 spine start 的自动吸附、异常 planar face rebuild、由 mesh 法线反推 BRep 正确性。
4. 现有 expected 只覆盖历史行，缺少针对用户失败样例的 native oracle、current mismatch 与 regression fixture。

## FreeCAD source authority

| 语义 | FreeCAD source | C12-M12 用法 |
| --- | --- | --- |
| PartDesign Pipe 主流程 | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::execute()` | Additive / Subtractive Pipe 的 profile、spine、sections、cap/sewing、Body Tip 与 Boolean owner 语义。 |
| Pipe algorithm setup | `src/Mod/PartDesign/App/FeaturePipe.cpp::Pipe::setupAlgorithm()` | `Mode=Fixed/Frenet/Auxiliary/Binormal`、transition、auxiliary correction、binormal 方向与 law 参数的源权威。 |
| Part Sweep wrapper | `src/Mod/Part/App/PartFeatures.cpp::Sweep::execute()` | Workbench `Part::Sweep` 的 section list、solid、frenet、transition 和 `makeElementPipeShell` 调用。 |
| TopoShape PipeShell | `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeElementPipeShell()` | `BRepOffsetAPI_MakePipeShell` 调用顺序、`Add` / `SetLaw` / `Build` / `MakeSolid` / history 生成。 |
| Python helper contract | `src/Mod/Part/App/BRepOffsetAPI_MakePipeShellPyImp.cpp` | advanced helper 的 public API：frenet、trihedron、binormal、spine support、auxiliary spine、tolerance、transition、simulate。 |

## cad-core 落点

| 落点 | 角色 |
| --- | --- |
| `cad-core/src/part/topo_shape_expansion.cpp` | PipeShell shared builder，必须对齐 FreeCAD `makeElementPipeShell` 与 PartDesign cap/sewing history。 |
| `cad-core/src/part/part_sweep.cpp` | `Part::Sweep` request parser、advanced DTO、response history 和 diagnostics。 |
| `cad-core/src/part_design/feature_pipe.cpp` | `PartDesign::AdditivePipe` / `SubtractivePipe` executor、Body replay、Boolean add/cut、profile/spine link 解析。 |
| `cad-core/src/runtime/feature_registry.cpp` | Feature type registration 与 request graph dispatch。 |
| `cad-core/tests/test_p7_features.py` | PartDesign Pipe focused regression。 |
| `cad-core/tests/test_p8_features.py` | Part Workbench Sweep focused regression。 |
| `cad-core/fixtures/c3m4`、`c4m2`、`c5m3`、`c51m4`、`c5m10`、`c5m12`、`c6m1`、`c6m3`、`c6m4` | 现有 sweep/pipe expected 与历史产品契约。 |

## 迁移原则

- FreeCAD source 是第一权威；旧 `chili3d` C++、当前前端 preview 或 mesh exporter 只能作为 drift evidence。
- `Standard` mode 不得被强行改成 `Frenet`；只有 FreeCAD source 和 request property 要求时才调用对应 `SetMode`。
- solid/cap/sewing 要复刻 FreeCAD，而不是用后处理补面来掩盖错误 profile/spine 关系。
- 用户失败样例必须成为 regression oracle：先记录 input/output/current mismatch，再决定最小实现。
- mesh 法线正确只能作为 response quality gate，不能替代 BRep shape / history parity。

## 入口

- 总入口：`7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次总入口.md`
- 方案：`7-3-20-16-C12-M12-FreeCADSweepPipeParity迁移批次方案.md`
- 工作步骤：`工作步骤细分/`
- 矩阵：`矩阵/`

## 预期出口

1. `implementation_package_authorized`：source authority、drift audit、oracle red loop 同时成立，允许修改 `cad-core/src`、fixtures、expected、tests。
2. `current_supported_with_regression_added`：用户失败样例经 native/current 验证不是 backend mismatch，只补 regression 或说明。
3. `blocked_by_missing_native_oracle`：FreeCADCmd 或 native expected 无法稳定产出，先保留 blocker，不写 C++。
4. `split_frontend_consumer_followup`：后端 BRep/response 已对齐，问题落在 `my-chili3d` consumer 或 preview。

## 非目标

- 不复制旧 `chili3d` 的 always Frenet sweep。
- 不用 profile 自动吸附、planar side face rebuild 或 mesh normal split 伪造 FreeCAD parity。
- 不在没有 native/current mismatch 的情况下大改 `cad-core`。
- 不改前端消费逻辑；若证据指向前端，另开 frontend package。
- 不解决完整 Topological Naming；只做 Sweep / Pipe 迁移所需的 history 与 subshape response。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore12.0/C12-M12-FreeCADSweepPipeParity迁移批次 docs/CADCore12.0/README.md
git diff --check
```
