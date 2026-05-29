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

## Fixture / expected 现状

当前 `fixtures/p2` 同时包含成功几何 fixture 和错误诊断 fixture。`expected/` 目录只承载成功几何 fixture 的 FreeCAD / oracle golden；错误 fixture 只在 diagnostics 矩阵中固定 code，不新增空 expected 文件。统一迁移规则见 `docs/5-30-04-33-CADCore-fixture-expected迁移方案.md`。

2026-05-30 迁移后的 P2 fixture 清单：

| fixture | 类型 | 当前验收方式 | expected 依据 |
| --- | --- | --- | --- |
| `rect-pad-pocket` | 成功几何，Pad 后 Pocket cut | diagnostics 为空；读取 `expected/rect-pad-pocket.freecad.json` 校验 object、bbox、volume、topology count | `FreeCADCmd` geometry-equivalent `Part.makeBox(10, 5, 10).cut(Part.makeBox(6, 3, 10, FreeCAD.Vector(2, 1, 0)))` |
| `body-basefeature-pad` | 成功几何，Body 从 `BaseFeature` 起步再执行 Pad | diagnostics 为空；读取 `expected/body-basefeature-pad.freecad.json` 校验 object、bbox、volume、topology count | `FreeCADCmd Part.makeBox(10, 5, 5)`，与 `fixtures/p3a/expected/pad-up-to-face.freecad.json` 使用同源 geometry-equivalent oracle |
| `missing-basefeature` | 错误诊断 | `missing_link_target` | 不进入 expected |
| `pocket-without-base` | 错误诊断 | `execution_failed` | 不进入 expected |
| `pocket-open-sketch` | 错误诊断 | `open_profile` | 不进入 expected |
| `unsupported-pocket-type` | 错误诊断 | `unsupported_property` | 不进入 expected |

迁移后 `test_p2_fixture_diagnostics` 继续作为 P2 全量错误码矩阵；两个成功几何 fixture 的几何 golden 不再写在测试代码里。

## 验收

- `fixtures/p2` 覆盖 FeatureBase、Pocket subtractive、Body BaseFeature 和错误 diagnostics。
- 成功几何 fixture 使用 expected 文件承载 FreeCAD / oracle golden；错误 fixture 只固定 diagnostics code。
- P1 Pad fixture 不回退。
