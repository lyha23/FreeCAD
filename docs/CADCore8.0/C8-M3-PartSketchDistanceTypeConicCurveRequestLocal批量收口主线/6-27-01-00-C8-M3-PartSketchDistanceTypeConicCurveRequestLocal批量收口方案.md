# C8-M3 Part / Sketch / DistanceType Conic Curve Request-Local 批量收口方案

## 背景

C8-M2 已经确认 `SubShapeBinder BindCopyOnChange` full temporary-document cache 不能从当前 native evidence 推导出稳定 request-local DTO，因此关闭为 no-code release gate。继续在 C8-M2 内扩展会违反无状态 CAD Core 边界。

当前 live capability 中仍有 active `remaining_gaps` 的可推进项是 `part_workbench.conic_curves`。但它不应只做 “删三个 gap” 的薄收口。`PartConicCurveDTO`、Sketcher conic geometry 和 DistanceType reference classification 都围绕 conic curve / reference API，因此本轮按批量语义闭环设计。

## 当前能力

- 已有 `PartConicCurveDTO`，payload key 为 `partGeometryCurve` / `partGeometryCurveConsumers`。
- 已覆盖 `Part.Hyperbola` / `Part.Parabola` finite edge、typed metadata、invalid diagnostics。
- 已有 Part consumer：`Part::Extrusion` 和 `Part::RuledSurface`。
- `cad-core` Sketcher 已存在 ArcOfHyperbola / ArcOfParabola parsing/building/external-reference 相关代码。
- Assembly / Ondsel distance type 已有 basic / extended reference classification 流程，但 conic curves capability 仍保留 `distance_type_default_todo`。

## 最小完整语义批次

| 批次 | 范围 | 预期处理 |
| --- | --- | --- |
| Part conic producer | Hyperbola / Parabola edge, metadata, diagnostics | 复核 existing fixtures；必要时补代表变体而不是单 case |
| Part consumer | Extrusion / RuledSurface 已有证据；评估是否需要同类 consumer 扩展 | 保持或扩展 request-local consumer contract |
| Sketcher conic input | ArcOfHyperbola / ArcOfParabola geometry、profile、external reference | 不做 full solver；补 solver-facing 状态 / diagnostics / fixture 边界 |
| DistanceType | default / todo 与 conic / curve reference 分类 | S5 裁决实现、重分类或删除 active gap |
| GUI / full solver | GUI conic edit、完整 Sketcher conic constraints | 发布 non-goal / reopen condition |

## S0 live 基线与范围冻结

冻结 C8-M3 的声明口径：不注册 fake `Part::Hyperbola` / `Part::Parabola` DocumentObject，不声明 GUI conic editor，不实现完整 Sketcher solver，只处理 request-local geometry / reference / diagnostics / capability。

## S1 FreeCAD source 与 current 覆盖复核

复核 `Geometry.cpp`、`Geometry2d.cpp`、Sketcher source、Assembly DistanceType source、current `cad-core` Part / Sketcher / Assembly / tests / capability，补全 source candidates。

## S2 scope 准入与 blocker 矩阵

把每条候选路线分类为 `already_supported`、`oracle_candidate`、`backend_gap_candidate`、`non_goal`、`capability_publication_gap` 或 `split_required`。只有有 FreeCAD authority + current mismatch + focused test route 的项才进入 S6 implementation。

## S3 PartConicCurveDTO 生产 / 消费 oracle 批量复核

复核既有 p8 conic fixtures，并判断是否需要同轮补一组代表 consumer / invalid / metadata fixtures。不得只新增一个容易过的 fixture。

## S4 Sketcher conic 输入与 solver 边界

复核 ArcOfHyperbola / ArcOfParabola 的 sketch input、profile、external reference、solver-facing 状态。若只是 full solver constraint semantic，发布为 non-goal；若缺 request-local input / diagnostics，则进入 S6。

## S5 DistanceType default 分类与 capability 准入

查清 `distance_type_default_todo` 是否是 stale publication gap、Assembly/Ondsel distance type 实现缺口、或 conic reference classification gap。能实现则准备 S6 C++ / tests；不能实现则写成明确 non-goal / oracle blocker。

## S6 实现与发布闸门

消费 S2-S5 证据。若存在 implementable `backend_gap_candidate`，按 C++ landing + focused tests 实现；若无实现缺口，则发布 no-code release gate，更新 capability/docs/tests，确保 active `remaining_gaps` 不再保留无解释项。

## 验收分层

本轮短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/工作步骤细分 --format markdown
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore8.0/C8-M3-PartSketchDistanceTypeConicCurveRequestLocal批量收口主线/矩阵/*.tsv
git diff --check
```

实现短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p8_features tests.test_p5_sketch tests.test_adapters tests.test_diagnostics
```

重型收口只在 S6 修改 shared Sketcher profile、DistanceType reference classifier、Part conic producer、expected collector 或 capability shared schema 时执行。
