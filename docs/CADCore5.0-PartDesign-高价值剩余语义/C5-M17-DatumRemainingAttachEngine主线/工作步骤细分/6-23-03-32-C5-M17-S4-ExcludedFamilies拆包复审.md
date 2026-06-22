# C5-M17-S4 ExcludedFamilies 拆包复审

## 目标

冻结 `Folding`、`IntersectionPoint`、`TangentU/V` 不进入 conic S6 的证据，并为后续独立包保留清晰 reopen condition。

## 拆包依据

| family | FreeCAD 入口 | 拆包原因 |
| --- | --- | --- |
| `Folding` | `Attacher.cpp:1947-2056` | 四 line fold-angle 状态机，依赖 shared vertex、edge order 和角度求解 |
| `IntersectionPoint` | `Attacher.cpp:2432,2703+` | face/face route、交线/交点 DTO 与 diagnostics 需单独 expected |
| `TangentU/V` | `Attacher.cpp:1652-1661` | surface tangent branch，和 TangentPlane surface normal 更接近，不属于 conic landmark |

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
```

## 非目标

- 不设计 `Folding` fixture。
- 不设计 `IntersectionPoint` fixture。
- 不发布 `TangentU/V` capability。
