# P2：FeatureBase + FeatureAddSub + Pocket

P2 的目标不是多做几个 feature 名字，而是把 PartDesign Body 的主链补出来：每个特征能拿到前序 solid，加料/减料能走统一通道，Pad 和 Pocket 共用同一套拉伸逻辑。

## FreeCAD 语义参考

| 能力 | FreeCAD 参考位置 |
| --- | --- |
| Body 顺序、Tip、BaseFeature | `src/Mod/PartDesign/App/Body.cpp`、`src/Mod/PartDesign/App/Body.h` |
| PartDesign 基类 | `src/Mod/PartDesign/App/FeatureBase.cpp`、`src/Mod/PartDesign/App/FeatureBase.h` |
| 加料/减料通道 | `src/Mod/PartDesign/App/FeatureAddSub.cpp`、`src/Mod/PartDesign/App/FeatureAddSub.h` |
| 共享拉伸 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`、`src/Mod/PartDesign/App/FeatureExtrude.h` |
| Pad / Pocket 入口 | `src/Mod/PartDesign/App/FeaturePad.cpp`、`src/Mod/PartDesign/App/FeaturePocket.cpp` |

## Step 13：冻结 MVP 基线

做什么：

- 保留 P0/P1 的 CLI、fixtures、diagnostics code 和 OCCT 输出。
- 确认 `Sketch -> Body -> Pad` 仍然通过 FreeCAD 对照。
- 将 link parser 从只认 `PropertyLinkSub` 扩成统一 link normalizer。

验收：

- `legacy-lowercase.json` 继续返回 `parse_error`。
- `rect-pad.json` 的 bbox、volume、mesh summary、subshape count 不回退。
- `PropertyLink` / `PropertyLinkList` / `PropertyLinkSubList` 能形成 dependency edge。

## Step 14：实现 FeatureBase / BaseFeature

做什么：

- 给 PartDesign feature 增加前序 solid 读取入口。
- Body 在执行当前 feature 前能找到它的 base shape。
- 支持 Body 的 `BaseFeature` 作为链起点。
- suppressed / invalid base 先返回 diagnostics，不做静默跳过。

验收：

- `body-basefeature-pad.json`：BaseFeature + Pad 能生成最终 Body。
- `missing-basefeature.json`：BaseFeature 链接丢失时返回稳定 diagnostics。
- feature executor 不直接扫描全局 shape cache，只通过本次 `ComputeContext` 取结果。

## Step 15：实现 FeatureAddSub 双通道

做什么：

- 抽出 `FeatureAddSub` 执行结果：`add_shape` / `sub_shape`。
- Pad 写 `add_shape`，Pocket 写 `sub_shape`。
- Body 只负责按顺序把 add_shape Fuse 到 base，或把 sub_shape Cut 掉。
- add/sub shape 只存在于一次 recompute 内，不写回持久文档。

验收：

- 单 Pad 仍返回和 P1 一致的最终 solid。
- sub_shape 单独生成失败时，Body 不生成假成功结果。
- diagnostics 能指出失败对象和属性。

## Step 16：抽 shared FeatureExtrude

做什么：

- 把 Profile、Length、Reversed、SideType 的通用拉伸逻辑从 Pad 移入 `FeatureExtrude`。
- Pad / Pocket 只负责选择 additive 或 subtractive 语义。
- P2 仍只要求 `Type=Length`；UpToFace、ThroughAll、TwoLengths 继续报 unsupported diagnostics。

验收：

- Pad 走 `FeatureExtrude` 后，P1 fixtures 输出不变。
- Pocket 能复用同一套 profile face 到 prism solid 的路径。
- 不支持的 `Type`、`SideType`、UpTo 参数返回 `unsupported_property`。

## Step 17：实现 Pocket Length subtractive

做什么：

- 注册 `PartDesign::Pocket`。
- 读取 `Profile`、`Length`、`Reversed`、`SideType`。
- 生成 subtractive prism。
- 由 Body 对前序 solid 执行 Cut，最终 Body Tip 指向 Cut 后 shape。

验收：

- `rect-pad-pocket.json`：矩形 Pad 上开 Pocket，最终体积和 FreeCAD 对照一致。
- `pocket-without-base.json`：没有前序 solid 时返回 diagnostics。
- `pocket-open-sketch.json`：开口草图不生成 subtractive shape。

## Step 18：固化 P2 fixtures

最小 fixtures：

```text
fixtures/p2/
  body-basefeature-pad.json
  rect-pad-pocket.json
  pocket-without-base.json
  pocket-open-sketch.json
  unsupported-pocket-type.json
  expected/
    rect-pad-pocket.freecad.json
```

完成定义：

- P1 所有 fixtures 继续通过。
- P2 正常 case diagnostics 为空。
- P2 错误 case diagnostics code 稳定。
- Pocket 的最终 Body bbox、volume 或 mesh 摘要能和 FreeCAD 对照。

## 暂缓范围

- `ThroughAll`、`UpToFace`、`UpToLast`、`TwoLengths`。
- Hole、Fillet、Chamfer、Refine。
- LinearPattern、Mirror、PolarPattern、MultiTransform。
- 完整 attachment/support/subname 恢复。
- Web / WASM / Worker adapter 产品化。
