# P2：FeatureBase + FeatureAddSub + Pocket

P2 当前作为已落地基线冻结：它证明 `cad-core` 已经不只是单个 Pad 几何，而是具备最小 PartDesign Body 主链。后续不要继续在 P2 文档里追加新 feature；从 `05-P3a-FeatureExtrude-UpTo终止语义.md` 开始扩 FreeCAD 终止语义。

## 当前验收结论

当前 `/cad-core` 已完成 P2 最小主链：

- link normalizer 支持 `PropertyLink`、`PropertyLinkList`、`PropertyLinkSub`、`PropertyLinkSubList`，并参与 recompute dependency edge。
- `PartDesign::FeatureBase` 已注册，能从 `BaseFeature` 链接读取本次 `ComputeContext` 内的前序 solid。
- Pad / Pocket 共享 `FeatureExtrude` 的 `Type=Length` 拉伸路径。
- Pad 写 `add_shape`，Pocket 写 `sub_shape`；Body 按 `Group` 顺序执行 Fuse / Cut。
- add/sub shape 只存在于一次 recompute 内，不写回持久文档。
- `rect-pad-pocket.json` 能生成 Pocket 后 Body 的 mesh、bbox、volume、subshape map。

P2 覆盖的 fixture：

```text
fixtures/p2/
  body-basefeature-pad.json
  missing-basefeature.json
  pocket-open-sketch.json
  pocket-without-base.json
  rect-pad-pocket.json
  unsupported-pocket-type.json
  expected/
    rect-pad-pocket.freecad.json
```

## 冻结规则

P2 完成后保持这些边界不再回退：

- `FeatureBase` 负责提供 Body 链起点，不让 Pad / Pocket 自己寻找全局 base solid。
- `FeatureAddSub` 保持双通道：`add_shape` / `sub_shape`。
- Body 是唯一负责把 add/sub shape 合成最终 solid 的位置。
- Pad / Pocket 只负责各自参数入口和 additive / subtractive 选择，不复制拉伸实现。
- 不把裸 `TopoDS_Shape` 写进持久 JSON。
- 任何跨 feature 的几何引用都必须通过 `PropertyLink*` 和本次 `ComputeContext` 解析。

## 已验证命令

```bash
cmake -S cad-core -B cad-core/build
cmake --build cad-core/build
python3 -m unittest discover -s cad-core/tests
./cad-core/cad-core recompute cad-core/fixtures/p2/rect-pad-pocket.json --output cad-core/out/rect-pad-pocket.result.json
```

现有测试覆盖 P0/P1/P2：

- MVP diagnostics。
- P2 diagnostics。
- `rect-pad` OCCT mesh / subshape map。
- `rect-pad-pocket` Cut 后 Body 结果。
- `body-basefeature-pad` 的 BaseFeature solid 使用。

## FreeCAD 语义来源

| 能力 | FreeCAD 参考位置 |
| --- | --- |
| Body 顺序、Tip、BaseFeature | `src/Mod/PartDesign/App/Body.cpp`、`src/Mod/PartDesign/App/Body.h` |
| PartDesign 基类 | `src/Mod/PartDesign/App/FeatureBase.cpp`、`src/Mod/PartDesign/App/FeatureBase.h` |
| 加料/减料通道 | `src/Mod/PartDesign/App/FeatureAddSub.cpp`、`src/Mod/PartDesign/App/FeatureAddSub.h` |
| 共享拉伸 | `src/Mod/PartDesign/App/FeatureExtrude.cpp`、`src/Mod/PartDesign/App/FeatureExtrude.h` |
| Pad / Pocket 入口 | `src/Mod/PartDesign/App/FeaturePad.cpp`、`src/Mod/PartDesign/App/FeaturePocket.cpp` |

## P2 冻结边界

P2 仍然只是 FreeCAD PartDesign 的最小主链，不等于完整 Body 生态。下面这些不属于 P2 本身，而由 P3a 之后的阶段继续承接：

- `FeatureExtrude` 的 P3a 路径已经继续扩出 `ThroughAll`、`UpToFace`、单目标 `UpToShape`，但 P2 只冻结 `Type=Length` + `SideType=One side` 的主链。
- `UpToFirst`、`UpToLast`、多 face / shell `UpToShape` 仍未完整对齐。
- `Two sides`、`Symmetric`、taper、custom direction、ReferenceAxis、object-local placement 由 P3b 处理。
- Body 还没有完整 placement、suppressed、refine、历史 topo naming 更新。

## 下一阶段入口

P2 之后先做 `FeatureExtrude` 终止语义，不先做 Hole、Fillet、Chamfer、Pattern、Mirror。

```text
P2 frozen baseline
  -> P3a FeatureExtrude ThroughAll
  -> P3a UpToFace / UpToShape subshape resolution
  -> P3a Pad/Pocket UpTo fixtures
  -> 06-P3b FeatureExtrude 双侧 / taper / custom direction / placement
  -> 07-P4 Document / Property / Placement 完整化
  -> 08-P5 Sketcher 核心与内部元素
  -> 09-P6 Topo Naming 主路径
  -> 10-P7 PartDesign 常用生态
```

原因：

- Pocket 现在只是 shared `FeatureExtrude` 的 thin wrapper；继续扩 Pocket 应落在 `FeatureExtrude`。
- `UpToFace` 会倒逼 `PropertyLinkSub` 指向真实 `FaceN`，这是 Fillet / Chamfer / Pattern 继续扩展前必须具备的能力。
- 如果先做 Pattern / Mirror，会复制一个还不完整的 source feature，后面仍要返工。
