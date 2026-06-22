# C5-M15 Datum3DPlane AttachEngine 方案

状态：`s5_request_local_capability_review_verified__S6_pending`

## 当前基线

`part_design.datum_attachment` 已支持 selected Datum map modes 的基础族和 C5-M14 `ProximityPoint1/2`。当前 capability exact blocker 仍保留 `Translate`、`TangentPlane`、`ThreePointsPlane`、`ThreePointsNormal`。这四项都落在 `AttachEngine3D::_calculateAttachedPlacement()`，都通过 request-local `AttachmentSupport` 生成 placement，且可以用同一个 `c51m5` Datum AttachEngine fixture family 做 FreeCADCmd expected，因此应作为一轮最小完整语义批次，而不是继续拆成单 fixture。

## 范围

| 项 | 本包处理 | 说明 |
| --- | --- | --- |
| `Translate` | 是 | 单 vertex，position 加 `AttachmentOffset` position，rotation 保留原 placement |
| `ThreePointsPlane` | 是 | vertex / edge endpoint 收集三点，normal 为叉积，base 为 centroid |
| `ThreePointsNormal` | 是 | 法向为 p2 相对 p0-p1 的垂直分量并 reverse，base 为 p2 投影点 |
| `TangentPlane` | 是 | face+vertex 投影，surface normal，tangent U/V，支持 vertex-first through-vertex 行为 |
| invalid diagnostics | 是 | missing support、wrong shape type、coincident/collinear points、surface projection/normal failure |
| `AttachmentOffset` / `MapReversed` | 是 | 非 `Translate` 复用 placementFactory 组合；`Translate` 单独复核 inline offset |
| capability/docs | 是 | 发布时只移除本包四个 exact blockers |
| `Folding` | 否 | 四 edge fold angle 状态机，后续独立包 |
| curve frame / curvature / conic landmarks | 否 | D1/D2、curvature、conic property family，后续独立包 |
| `IntersectionPoint` | 否 | 需要先确认 FreeCAD direct branch / enum route |

## FreeCAD 调用链

1. `AttachEngine3D::_calculateAttachedPlacement()` 通过 `readLinks()` 把 `AttachmentSupport` 转成 request-local `TopoShape` 和 ref type。
2. `mmTranslate` 分支要求一个 vertex，直接返回 `Base::Placement`：position = vertex + attachment offset position，rotation = `origPlacement.getRotation()`。
3. `mmTangentPlane` 分支要求 face+vertex；若第一个 support 是 vertex，则 swap，并把 base point 固定在原 vertex，否则用 surface projector 的 nearest point。
4. `mmTangentPlane` 用 `GeomAPI_ProjectPointOnSurf` 得到 u/v，调用 `Tools::getNormal(face, u, v, Precision::Confusion(), SketchNormal, done)`，再用 tangent U 或 tangent V crossed normal 得到 X axis。
5. `mmThreePointsPlane` / `mmThreePointsNormal` 从 vertex 或 edge endpoints 收集前三点，处理 coincident / collinear diagnostics。
6. 非 `Translate` 分支进入通用 placement 组合；cad-core 不应在 adapter 或 fixture 后处理里修正 orientation。

## cad-core 实现边界

- `cad-core/src/part_design/datum_attachment.h`：
  - 增加 3D plane family 判定，确保四个 mode 能进入 selected placement 主路径。
  - 新增 `Translate` vertex helper，保留 FreeCAD 的 inline AttachmentOffset position 与 original rotation 语义。
  - 新增 three-point point collection helper，支持 vertex 和 edge endpoints，不按 fixture 名称或几何类型排序猜测。
  - 新增 tangent-plane helper，优先复刻 FreeCAD projection、normal、tangent 和 vertex-first base point 语义。
  - 输出结构化 diagnostics，不抛裸异常、不静默 fallback 到 default placement。
- `cad-core/fixtures/c51m5`：
  - 新增 success fixture：`partdesign-datum-3d-plane-modes.json`。
  - 新增 diagnostics fixture：`partdesign-datum-3d-plane-diagnostics.json`。
- `cad-core/tests/test_p7_features.py` / `test_expected_fixtures.py`：
  - expected parity 覆盖 `Translate`、三点三 vertex、edge endpoints + vertex、tangent plane face+vertex 与 vertex-first。
  - focused invalid 覆盖 missing support、wrong shape、coincident/collinear、surface projection/normal failure。
- `cad-core/src/adapters/c_api/c_api.cpp`：
  - 实现完成后补 fixtures/diagnostics/capability evidence，并只移除本包四个 exact blockers。

## 实施顺序

1. S0：冻结 live exact blocker、禁止声明和 C5-M14 done baseline。
2. S1：已记录 `AttachEngine3D::_calculateAttachedPlacement()` 四个分支和 excluded family 的源码证据；这只证明待实现语义，不晋级 supported。
3. S2：已把 scope、blocker、backendGap、fixture/oracle、nonGoal 路由清楚。
4. S3：已复审并准备实现 `Translate`、`ThreePointsPlane`、`ThreePointsNormal`；S6 直接按 one-vertex Translate、ordered ThreePoints collection、Plane centroid/cross normal、Normal projected base 和 invalid diagnostics 落代码/fixtures/tests。
5. S4：已复审并准备实现 `TangentPlane` surface projection / `Tools::getNormal` / tangent U/V / support order diagnostics；若 S6 无法 source-backed 复刻 normal fallback，必须拆 precise blocker。
6. S5：已复核 request-local placement response、AttachmentOffset / MapReversed、writeback 建议和 capability 发布边界；exact blocker 只能由 S6 expected/tests/docs 一起关闭。
7. S6：批量采集 FreeCADCmd expected，落 C++、fixtures、focused tests、capability/docs 和验收记录。

## 验收分层

本轮方案短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/README.md docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M15-Datum3DPlaneAttachEngine主线/工作步骤细分 --format markdown
```

实现短跑：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-3d-plane-modes.json --check
FREECADCMD=/home/user/.local/bin/freecadcmd python3 tools/collect_freecad_expected.py fixtures/c51m5/partdesign-datum-3d-plane-diagnostics.json --check
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_3d_plane_modes_match_expected tests.test_p7_features.CadCoreP7FeatureTest.test_c51x_datum_3d_plane_invalid_diagnostics
python3 -m unittest tests.test_adapters.CadCoreAdapterTest.test_c_api_capabilities_exposes_web_contract_facts
```

阶段收口：

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features tests.test_expected_fixtures tests.test_adapters
```

## 下一轮代码落点

| blocker | C++ 落点 | FreeCAD 依据 | tests | 成功标准 |
| --- | --- | --- | --- | --- |
| `C5M15-BLK-101` | `cad-core/src/part_design/datum_attachment.h` | `Attacher.cpp:1220,1363-1385` `mmTranslate` | `test_c51x_datum_3d_plane_modes_match_expected` | one vertex only；position = vertex + `attachmentOffset.getPosition()`；rotation = `origPlacement.getRotation()`；return before `placementFactory()` |
| `C5M15-BLK-201` | `datum_attachment.h` | `Attacher.cpp:1284-1300,1857-1945` `mmThreePointsPlane/Normal` | 同上 | support 顺序读取 vertex 点或 edge first/last endpoints，收满三点即停；Plane centroid/cross normal；Normal reversed perpendicular normal/projected base |
| `C5M15-BLK-202` | `datum_attachment.h` + diagnostics fixture | `Attacher.cpp:1890-1934` less-than-three / coincident / collinear checks | `test_c51x_datum_3d_plane_invalid_diagnostics` | less-than-three、coincident、collinear 有稳定 diagnostic code，不 fallback 到 default placement |
| `C5M15-BLK-301` | `datum_attachment.h` | `Attacher.cpp:1590` `mmTangentPlane`；`Tools.cpp:728,783` `Tools::getNormal` | modes + diagnostics tests | face+vertex、vertex-first、surface projection、`Tools::getNormal` 等价、tangent U/V、reversed face orientation 与 expected 一致 |
| `C5M15-BLK-501` | `c_api.cpp`、C5/C51 docs、matrices | capability exact blocker source | adapter capability test | exact blocker 只移除本包四个 mode，其它 excluded family 保留 |

禁止捷径：

- 不按 fixture 名称分支。
- 不用 bbox、面积、长度或 cad-core output 反推 expected。
- 不在 adapter 层实现 AttachEngine 业务逻辑。
- 不把 `TangentPlane` 简化成 planar face-only；必须保留 surface projection / normal / tangent 语义或把未复刻项写成精确 blocker。
- 不把 `Folding`、curve frame、curvature、conic landmark、`IntersectionPoint` 顺带声明 supported。
