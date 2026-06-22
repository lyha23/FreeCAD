# 【已实现】C5-M15-S5 requestLocalPlacementAndCapability 专项复审

状态：`s5_request_local_capability_review_verified`

## 目标

复核 C5-M15 在 CAD Core 无状态边界内的 placement response、AttachmentOffset / MapReversed、documentObjectUpdates、capability exact blocker 和 docs 发布口径。

## FreeCAD / cad-core 依据

- `~/Chili3DProject/FreeCAD/src/Mod/Part/App/AttachExtension.cpp::AttachExtension::positionBySupport()`：FreeCAD 计算 placement 后可能写回 support subnames。
- `~/Chili3DProject/FreeCAD/src/App/PropertyLinks.cpp`：`PropertyLinkSub/List` 提供 sub values 和 downstream reference 证据。
- `cad-core/src/part_design/datum_attachment.h`：selected MapMode request-local placement helper。
- `cad-core/src/adapters/c_api/c_api.cpp`：capability supported / diagnostics / exact_blockers 输出。

## 范围

| 边界 | C5-M15 处理 |
| --- | --- |
| request graph | 从 request 中读取 `AttachmentSupport` / `MapMode` / `AttachmentOffset` / `MapReversed` |
| placement output | response 返回 object placement / shape result；不保存 backend session |
| subname recovery | 仅返回 `documentObjectUpdates` / `elementReferenceUpdates` 建议 |
| capability | 实现完成后只移除四个 exact blockers |
| docs | C5 root、package matrix、C51X docs 状态一致 |

## S5 复审结论

| 边界 | 结论 | S6 要求 |
| --- | --- | --- |
| request-local graph | `datumAttachmentPlacement()` 只从当前 request 的 `AttachmentSupport`、`MapMode`、`AttachmentOffset`、`MapReversed` / `Reverse`、`MapPathParameter` 读取输入 | C5-M15 不新增 backend attachment session、shape cache 或完整 BREP state |
| support 解析 | 当前 line/proximity 已有多 support route；C5-M15 还需让 `Translate`、`ThreePoints*`、`TangentPlane` 按各自 support count / subshape 需求解析 | S6 必须同步 `requiresSubshape` 和 support count，尤其 `TangentPlane` 两 support 与 `ThreePoints*` 多点收集 |
| placement composition | `Translate` 是 FreeCAD 分支内 inline offset；非 `Translate` 走 selected placement 后再组合 `AttachmentOffset` / `MapReversed` | S6 不能统一套同一 offset 路径；必须按 S3/S4 差异落代码和 fixtures |
| writeback | `appendAttachmentSupportWriteback()` / `appendAttachmentSupportsWriteback()` 只返回 request-local update 建议 | response 可有 `documentObjectUpdates` / `elementReferenceUpdates`，不得持久保存 session 或 shape |
| capability | `c_api.cpp` 当前仍列出 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal` | S6 只有在 native expected、focused tests、adapter capability test 和 docs/root matrices 同步后，才能移除这四项 |
| excluded family | Folding、curve frame/curvature、conic landmarks、`IntersectionPoint`、GUI/session 仍非本包 | S6 发布时必须保留这些 blocker / non-goal，不得借 C5-M15 一并声明 supported |

## 必须回写的矩阵行

- `C5M15-SCOPE-501`。
- `C5M15-BLK-501`。
- `C5M15-ORC-501`。
- `C5M15-VAL-501` / `C5M15-VAL-502`。
- root matrices：`C5-SRC-015`、`C5-SCOPE-1501`、`C5-BLK-1501`、`C5-ORC-1501`、`C5-NG-018`、`C5-VAL-1501`、`C5-VAL-1502`。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD
rg -n 'C5M15-BLK-501|C5M15-ORC-501|C5-SCOPE-1501|C5-BLK-1501|C5-ORC-1501|C5-NG-018|C5-VAL-1501' docs/CADCore5.0-PartDesign-高价值剩余语义
rg -n 'datum_attach_engine_remaining_modes|Translate|TangentPlane|ThreePointsPlane|ThreePointsNormal' cad-core/src/adapters/c_api/c_api.cpp
```

验收标准：

- S5 不允许出现 persistent backend attachment session。
- exact blocker closeout 标准必须保留 excluded family。
- docs/capability closeout 与 S6 focused tests 绑定，不允许 docs-only 移除 blocker。
- S5 `【已实现】` 只代表发布边界复审完成；`C5M15-BLK-501` / root `C5-BLK-1501` 仍是 S6 release gate。

## 非目标

- 不实现 C++。
- 不运行 cad-core build；S6 或代码实现阶段再执行。
- 不修改 `cad-core/src/adapters/c_api/c_api.cpp` 的 `datum_attach_engine_remaining_modes`。
