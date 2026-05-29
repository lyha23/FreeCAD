# P7：PartDesign 常用生态

P7 的目标是在 P3-P6 底座稳定后，扩展前端参数化建模最常用的 Body 特征：Datum、Refine、Hole、Fillet、Chamfer、Pattern、Mirror、MultiTransform、Scaled。

## 前置条件

- P4 的 LinkSub / Placement 模型稳定。
- P6 的 `NamedShape` / `ElementMap` / MapperHistory 主路径可用。
- P3b 的 `FeatureExtrude` 方向、双侧和 placement 不回退。

不要在这些前置条件缺失时先铺开 Pattern / Mirror / DressUp，否则会复制不完整的 source feature 和 subshape 引用语义。

## FreeCAD 语义来源

| 能力 | FreeCAD 参考位置 |
| --- | --- |
| Datum | `src/Mod/PartDesign/App/Datum*.cpp` |
| Hole | `src/Mod/PartDesign/App/FeatureHole.cpp` |
| DressUp 基类 | `src/Mod/PartDesign/App/FeatureDressUp.cpp` |
| Fillet | `src/Mod/PartDesign/App/FeatureFillet.cpp` |
| Chamfer | `src/Mod/PartDesign/App/FeatureChamfer.cpp` |
| Refine | `src/Mod/PartDesign/App/FeatureRefine.cpp` |
| Transformed | `src/Mod/PartDesign/App/FeatureTransformed.cpp` |
| Pattern / Mirror / MultiTransform | `src/Mod/PartDesign/App/Feature*.cpp` 对应子类 |

## Step 44：Datum / Origin

目标：

- 支持 Datum Plane / Line / Point / CoordinateSystem。
- 支持 Body / Origin 下的基础坐标关系。
- Datum 可作为 sketch support、extrude direction、pattern axis。

fixtures：

```text
fixtures/p7/
  datum-plane-sketch-support.json
  datum-line-reference-axis.json
  datum-point-placement.json
```

## Step 45：Refine

目标：

- 迁移 FreeCAD refine maker / object chain。
- Refine 属性参与 Body 链和 topo naming。
- 删除输出端 refine fallback。

fixtures：

```text
fixtures/p7/
  body-refine-pad-pocket.json
  refine-reference-stability.json
```

## Step 46：Hole

目标：

- 支持常用 Hole 参数：Profile、Depth、Diameter、Type、Threading 基础边界。
- Hole 通过 Body subtractive 通道执行。
- Hole placement / support 与 Sketch / Datum 对齐。

fixtures：

```text
fixtures/p7/
  hole-simple-through.json
  hole-blind-depth.json
  hole-invalid-support.json
```

## Step 47：Fillet / Chamfer

目标：

- DressUp 通过 LinkSub 读取 base edge / face。
- 半径 / chamfer 参数进入 OCCT builder。
- 修改 base feature 后引用通过 P6 topo naming 恢复或 diagnostics。

fixtures：

```text
fixtures/p7/
  fillet-pad-edge.json
  chamfer-pad-edge.json
  fillet-reference-after-length-change.json
  chamfer-missing-edge.json
```

## Step 48：Transformed family

目标：

- 支持 `FeatureTransformed` 基础语义。
- LinearPattern、PolarPattern、Mirrored、MultiTransform、Scaled 复用 source feature。
- source feature 的 NamedShape / ElementMap history 传播到 transformed result。

fixtures：

```text
fixtures/p7/
  linear-pattern-pad.json
  polar-pattern-pocket.json
  mirrored-pad.json
  multi-transform-pad.json
  scaled-feature.json
```

## 完成定义

P7 完成需要同时满足：

- 每个支持的 PartDesign feature 有成功 fixture、错误 fixture、FreeCAD oracle。
- DressUp 和 Transformed family 都走统一 LinkSub / topo naming。
- Refine 不再靠输出 fallback。
- Body Tip / Group / BaseFeature 顺序和 FreeCAD 对齐。
- Pattern / Mirror 不复制不完整的 source feature 几何实现。
