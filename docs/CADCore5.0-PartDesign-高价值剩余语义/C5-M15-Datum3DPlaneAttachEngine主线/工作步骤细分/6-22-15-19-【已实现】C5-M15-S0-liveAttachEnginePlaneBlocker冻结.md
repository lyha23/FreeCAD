# 【已实现】C5-M15-S0 live AttachEngine plane blocker 冻结

状态：`s0_frozen_verified`

## 目标

冻结 C5-M15 的声明口径、live blocker、禁止声明和完成条件，确保后续实现按批量语义闭环推进。

## 输入

- `cad-core/src/adapters/c_api/c_api.cpp` 中 `datum_attach_engine_remaining_modes`。
- `cad-core/src/part_design/datum_attachment.h` 的 MapMode label 列表。
- `docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线` 的 done baseline。
- FreeCAD source：`~/Chili3DProject/FreeCAD/src/Mod/Part/App/Attacher.cpp::AttachEngine3D::_calculateAttachedPlacement()`。

## 声明口径

| 声明 | C5-M15 口径 |
| --- | --- |
| 本包支持对象 | `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal` |
| 批次理由 | 同一 FreeCAD 调用链、同一 `datum_attachment.h` selected placement 边界、同一 `c51m5` expected 家族 |
| 当前状态 | S0 live guard 已冻结；S1-S6 待执行 |
| 发布前提 | FreeCADCmd expected、C++ 实现、fixtures、focused tests、capability/docs、矩阵状态全部闭环 |

## 禁止声明

- 不声明 full AttachEngine3D supported。
- 不声明 `Folding`、Frenet/curve frame、curvature/conic landmark 或 `IntersectionPoint` supported。
- 不声明 `TangentPlane` 只要能对 planar face 成功就算完成；必须覆盖 surface projection / normal / tangent 或留下精确 blocker。
- 不把 C5-M14 ProximityPoint 的双 support 实现当成 C5-M15 已完成证据。
- 不把 cad-core output、bbox 或手写 expected 当成 oracle。

## S0 证据

- `datum_attach_engine_remaining_modes` live list 仍包含 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal`。
- `datum_attachment.h` MapMode label list 也包含这四个 label；这只证明 label 可识别，不证明 selected placement 已支持。
- FreeCAD `AttachEngine3D::_calculateAttachedPlacement()` 对四个 mode 有独立分支：`Translate` 是 vertex-local placement，`TangentPlane` 是 face+vertex projection / normal / tangent，`ThreePointsPlane` / `ThreePointsNormal` 是三点收集和法向推导。
- C5-M14 已收口证据只关闭 DatumPoint `ProximityPoint1/2`；它保留三点、折叠、曲线 frame / curvature、conic landmark 和 `IntersectionPoint` 等后续 blocker，不能作为 C5-M15 完成证据。

## 纳入 / 排除

| mode family | S0 决策 | 原因 |
| --- | --- | --- |
| `Translate` | 纳入 | 同一 `AttachEngine3D`，单 vertex placement，可与三点/tangent 共用 selected placement route |
| `ThreePointsPlane` / `ThreePointsNormal` | 纳入 | 同一三点收集和 placementFactory family |
| `TangentPlane` | 纳入 | 同一 face+vertex surface placement family，风险单独放 S4 |
| `Folding` | 排除 | 四 edge fold angle 状态机，调用链虽相邻但语义复杂度不同 |
| curve frame / curvature | 排除 | 依赖 D1/D2、Frenet normal、曲率半径和不同 failure mode |
| conic landmarks | 排除 | `Focus`、`Directrix`、`Asymptote` 属 conic property family |
| `IntersectionPoint` | 排除 | 需先确认 FreeCAD direct branch / enum route |

## 状态字典

| 状态 | 含义 |
| --- | --- |
| `pending_live_guard` | S0 之前的 live blocker 冻结状态，不改实现 |
| `s0_frozen_verified` | live blocker、MapMode label、FreeCAD source 分支、C5-M14 边界均已核对；仍不代表支持 |
| `pending_source_audit` | source candidate 待补证据 |
| `pending_backendGap` | FreeCAD 语义明确，cad-core 未实现 |
| `pending_native_oracle` | 需要 FreeCADCmd expected |
| `releaseGate` | 需要 capability/docs/tests 发布收口 |
| `nonGoal` | 不属于本包，必须保留 reopen 条件 |

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'Translate|TangentPlane|ThreePointsPlane|ThreePointsNormal|datum_attach_engine_remaining_modes' cad-core/src/adapters/c_api/c_api.cpp cad-core/src/part_design/datum_attachment.h
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
rg -n 'C5M15-SCOPE-101|C5M15-SCOPE-201|C5M15-SCOPE-202|C5M15-SCOPE-301|C5M15-NG-001|C5M15-NG-004' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵
```

验收标准：

- package docs 明确四个 in-scope mode 与 excluded family。
- blocker queue 至少包含 Translate、ThreePoints、TangentPlane、release gate 四类 blocker。
- non-goal registry 记录 Folding、curve frame/curvature、conic landmark、IntersectionPoint。
- 未修改 C++ 实现或 capability blocker。
- S0 `【已实现】` 只代表 docs-only freeze；不得升级为 supported、oracle done 或 release gate closed。

## 非目标

- 不采集 oracle。
- 不改 `datum_attachment.h`。
- 不调整 C5-M14 包内实现状态。
