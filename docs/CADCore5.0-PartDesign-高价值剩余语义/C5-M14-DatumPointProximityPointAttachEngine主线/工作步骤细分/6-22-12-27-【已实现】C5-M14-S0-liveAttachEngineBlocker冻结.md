# 【已实现】C5-M14-S0 live AttachEngine Blocker 冻结

## 目标

冻结 C5-M14 的 live 口径：只处理 DatumPoint `ProximityPoint1` / `ProximityPoint2`，并确认它们仍在 `datum_attach_engine_remaining_modes` exact blocker 中。

## 声明口径

| 声明 | 允许状态 | 依据 |
| --- | --- | --- |
| `ProximityPoint1/2` | `pending_backendGap` | capability blocker 仍列出两项，FreeCAD source 有直接实现 |
| `IntersectionPoint` | `nonGoal_source_unclear` | 当前 `AttachEnginePoint::_calculateAttachedPlacement()` 未见直接 case |
| DatumLine line-family | `done_existing` | 已由 C51X 上一批 supported |
| C5-M13 Part Workbench surface | `out_of_scope` | 本包不触碰 surface narrowed blockers |

## 禁止声明

- 禁止声明完整 Datum AttachEngine supported。
- 禁止把 `ProximityPoint1/2` 与 `IntersectionPoint` 合并实现。
- 禁止把 FreeCAD edge-face intersection 特殊路径降级成普通 distance fallback。
- 禁止写跨请求 attachment session 或长期 shape cache。

## live 验证记录

- `pwd`：`/home/user/Chili3DProject/FreeCAD`。
- HEAD：`afc654d6d1`，`afc654d6d1 feat: 支持 DatumLine line-family 附着模式`。
- 工作区：进入 S0 前已有 `cad-core/`、C5.0 根矩阵和整套 C5-M14 未提交/未跟踪文件；本步骤只允许回写 C5-M14 主线文档与矩阵。
- blocker 证据：`cad-core/src/adapters/c_api/c_api.cpp:2142` 仍有 `datum_attach_engine_remaining_modes`，`cad-core/src/adapters/c_api/c_api.cpp:2167-2168` 仍列出 `ProximityPoint1` / `ProximityPoint2`。
- 结论：S0 只冻结 live blocker 与 non-goal 口径，`ProximityPoint1/2` 仍是待实现 backendGap；不得写成 supported 或 releaseGate closed。

## 必须回写的矩阵行

- `C5M14-SCOPE-000`
- `C5M14-BLK-000`
- `C5M14-ORC-001`
- `C5M14-NG-001` 到 `C5M14-NG-006`

## 验收标准

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n '"ProximityPoint1"|"ProximityPoint2"|datum_attach_engine_remaining_modes' cad-core/src/adapters/c_api/c_api.cpp
rg -n 'C5M14-SCOPE-000|C5M14-BLK-000|C5M14-ORC-001' docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线/矩阵
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M14-DatumPointProximityPointAttachEngine主线
```

通过条件：

- capability blocker 中仍可定位 `ProximityPoint1/2`。
- S0 矩阵行存在，状态不写成 supported。
- non-goal 明确排除 `IntersectionPoint`、GUI、cross-request session、C5-M13 surface。

## 非目标

- 不采集 expected。
- 不写 C++。
- 不更新 capability supported 列表。
