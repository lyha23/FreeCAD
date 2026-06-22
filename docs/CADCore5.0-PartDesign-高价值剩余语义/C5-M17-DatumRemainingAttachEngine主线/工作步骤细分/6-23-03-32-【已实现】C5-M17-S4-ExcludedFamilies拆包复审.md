# 【已实现】C5-M17-S4 ExcludedFamilies 拆包复审

状态：`done_s4_excluded_families_frozen`

## 目标

冻结 `Folding`、`IntersectionPoint`、`TangentU/V` 不进入 conic S6 的证据，并为后续独立包保留清晰 reopen condition。

## 拆包依据

| family | FreeCAD 入口 | 拆包原因 |
| --- | --- | --- |
| `Folding` | `Attacher.cpp:1947-2056` | 四 line fold-angle 状态机，依赖 shared vertex、edge order 和角度求解 |
| `IntersectionPoint` | `Attacher.cpp:2432,2703+` | face/face route、交线/交点 DTO 与 diagnostics 需单独 expected |
| `TangentU/V` | `Attacher.cpp:1652-1661` | surface tangent branch，和 TangentPlane surface normal 更接近，不属于 conic landmark |

## S4 结论

- `Folding` 不进入 conic S6：`Attacher.cpp:1313` 只注册四条 `rtLine`，`Attacher.cpp:1947-2056` 固定输入顺序为 `edgeA, axisA, axisB, edgeB`，要求四条 line 共享顶点并用 sign 调整方向，随后调用 `calculateFoldAngle()`；`Attacher.cpp:2309-2346` 还包含折叠轴平行、edge/axis 平行和折叠角余弦异常 diagnostics。
- `IntersectionPoint` 不进入 conic S6：`Attacher.cpp:2432` 注册的是 `rtFace, rtFace`，`Attacher.cpp:2703-2752` 走 `BRep_Tool::Surface` + `GeomAPI_IntSS`，要求恰好一条 straight intersection curve，后续必须单独证明 face/face DTO/API、交线/交点 expected 和 diagnostics。
- `TangentU/V` 不进入 conic S6：`Attacher.h:83-84` 把它们列为 line map modes，但 `Attacher.cpp:1644-1668` 中实际使用 `GeomLProp_SLProps::TangentU/TangentV` 选择 TangentPlane surface tangent 方向，属于 surface tangent 分支，不是 conic line/point landmark。

## 必做

1. 更新 package-local non-goal registry。
2. 更新 backend gap classification，把上述 family 从 conic S6 中排除。
3. 更新 root non-goal row，说明 C5-M17 只打开 conic first batch。
4. 写清后续包 reopen condition：source route、DTO/API、native expected、focused tests。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD
rg -n 'mmFolding|mm1Intersection|TangentU|TangentV' src/Mod/Part/App/Attacher.cpp src/Mod/Part/App/Attacher.h
git diff --check -- docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线 docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵
for f in docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/矩阵/*.tsv docs/CADCore5.0-PartDesign-高价值剩余语义/矩阵/*.tsv; do awk -F '\t' 'NR==1{n=NF} NF!=n{print FILENAME ":" NR ": expected " n " fields, got " NF}' "$f"; done
python3 ~/.codex/skills/goal-step-runner/scripts/step_goal_queue.py docs/CADCore5.0-PartDesign-高价值剩余语义/C5-M17-DatumRemainingAttachEngine主线/工作步骤细分 --format markdown
```

## 非目标

- 不设计 `Folding` fixture。
- 不设计 `IntersectionPoint` fixture。
- 不发布 `TangentU/V` capability。
- 不改 C++。
- 不采集 oracle。
- 不改 capability exact blocker。
