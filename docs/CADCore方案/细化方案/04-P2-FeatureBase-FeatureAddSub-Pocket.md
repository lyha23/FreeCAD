# P2：FeatureBase、FeatureAddSub、Pocket

P2 固定 PartDesign Body 的加料 / 减料主链，避免 Pad、Pocket 和后续 feature 各自组合最终 Body。

## 当前基线

- `PartDesign::FeatureBase` 可暴露前序 base solid。
- Pad / Pocket 共享 `FeatureExtrude`。
- executor 产出 `addSubShapes[feature].addShape` 或 `subShape`。
- Body 按 Group 顺序和 Tip 执行 fuse / cut，并输出最终 solid。
- `BaseFeature` 可作为 Body 初始 solid。

## 模块责任

| 模块 | 责任 |
| --- | --- |
| `features/feature_base.*` | 读取 base shape，不做 boolean |
| `features/feature_extrude.*` | 生成 add/sub tool shape |
| `features/pad.*` | Pad 属性边界和 additive 通道 |
| `features/pocket.*` | Pocket 属性边界和 subtractive 通道 |
| `features/body.*` | Body 内顺序、Tip、fuse / cut、最终导出 |
| `topo/named_shape.*` | Body boolean history 子集 |

## FreeCAD 依据

- `src/Mod/PartDesign/App/Body.cpp`
- `src/Mod/PartDesign/App/FeatureBase.cpp`
- `src/Mod/PartDesign/App/FeatureAddSub.cpp`
- `src/Mod/PartDesign/App/FeaturePad.cpp`
- `src/Mod/PartDesign/App/FeaturePocket.cpp`

## 保持规则

- Body 承担最终组合，feature 不直接生成 Body 结果。
- Pad / Pocket 继续共享 `FeatureExtrude`。
- 缺失 base、无 profile、unsupported type 必须 diagnostics。
- Body boolean source 应继续进入 `NamedShape` / `ElementMap` 传播。

## 验收

- `fixtures/p2` 覆盖 FeatureBase、Pocket subtractive、Body BaseFeature 和错误 diagnostics。
- P1 Pad fixture 不回退。
