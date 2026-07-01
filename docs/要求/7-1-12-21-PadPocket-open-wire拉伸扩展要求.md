# Pad/Pocket open wire 拉伸扩展要求

## 背景

FreeCAD native 的 PartDesign Pad / Pocket 语义仍然是 face-first：

- FreeCAD `src/Mod/PartDesign/App/FeaturePad.cpp::Pad::execute()` 调用 `buildExtrusion(ExtrudeOption::MakeFace | ExtrudeOption::MakeFuse)`。
- FreeCAD `src/Mod/PartDesign/App/FeaturePocket.cpp::Pocket::execute()` 使用 `MakeFace | MakeFuse | InverseDirection`，目标是生成可参与 Body fuse / cut 的 solid。
- FreeCAD `src/Mod/PartDesign/App/FeatureExtrude.cpp::FeatureExtrude::buildExtrusion()` 在 `makeface == true` 时通过 `getTopoShapeVerifiedFace()` 获取 profile face；open wire 不能直接成为 Pad / Pocket 的有效 profile face。
- FreeCAD `src/Mod/Part/App/FeatureExtrusion.cpp::Extrusion::execute()` 是 Part 工作台的通用拉伸路径；`Solid=false` 时可以把 wire / edge 作为 shape 拉伸成面或 shell，`Solid=true` 才尝试从 wires 造 face。

所以“Pad/Pocket 默认支持 open wire”不是 FreeCAD native 行为，而是 `cad-core` 的产品扩展。本文要求 `cad-core` 的默认请求路径直接支持 open wire；FreeCAD native 结果只作为严格兼容模式和 oracle 对照，不再作为 `cad-core` 默认行为。

## 大白话解释

闭合草图轮廓像一块纸片，Pad 把它拉成实体，Pocket 把它拉成刀具去减料。

open wire 只是一条没封口的线。直接拉伸它，通常得到的是一张面或一片壳，不是实体。这个结果可以显示、拾取、参与后续引用，但不能天然拿去和 Body 做实体 fuse / cut。

因此支持 open wire 要拆成两层：

1. open wire 拉伸显示：复用 Part::Extrusion 的线 / wire 拉伸能力，生成 face / shell / compound。
2. open wire 实体加料或减料：必须额外定义厚度、偏置侧、端帽和布尔规则，生成真正的切削 solid。

默认支持 open wire 的第一层语义是：Pad / Pocket 都能把 open wire 拉伸成可显示、可拾取、可追溯的 surface / shell / compound。第二层实体语义仍然要显式：Pad 要真正加料需要 thin solid，Pocket 要真正减料需要 thin cut 或 surface split policy。

## 目标

1. `cad-core` 后端默认支持 Pad / Pocket open wire profile，不要求前端显式 opt-in。
2. 默认行为改为 `Auto`：closed face 走现有 solid Pad / Pocket；open wire 走 surface extrusion，生成可显示、可拾取的 feature shape。
3. open wire 路径复用 `cad-core/src/part/part_extrusion.cpp` 的 Part::Extrusion 几何能力，但不要让前端把 Pad / Pocket 偷换成 Part::Extrusion 对象。
4. profile 解析必须支持 raw Sketch open edge / wire 的稳定身份，例如 `g101`，不能靠 `InternalFaceN`、`InternalEdgeN`、`OpenWireN` 或输出排序猜。
5. 对 Body fuse / cut 的参与策略必须显式返回：默认 surface extrusion 是 `display_only`，只有 thin solid / thin cut / surface split policy 才能修改 Body solid。

## 非目标

- 不把 FreeCAD native Pad / Pocket 的 expected 改成成功；`.freecad.json` 仍记录 FreeCAD 原生不支持 open wire 的事实。
- 不让 collector 把 open wire Pad / Pocket 伪装成 FreeCAD native 成功。
- 不让前端用隐藏 Part::Extrusion 替换用户建的 Pad / Pocket。
- 不伪造 `InternalFaceN`，不把 open-only sketch 的空 `InternalShape` 当错误修正掉。
- 不在 adapter、runtime 输出层或 sketch executor 里靠 source index、split 顺序、几何类型排序猜稳定名。
- 不在 Pocket open wire 上默认做实体减料；没有厚度或 split policy 时，默认仍生成 Pocket 自身的 surface extrusion shape，但 Body participation 必须是 `display_only`。

## 输入合同

新增一个可选的 `cad-core` 扩展属性，建议名为 `OpenProfileMode`。它属于 Pad / Pocket executor 的后端扩展，不是 FreeCAD native 属性。

默认请求可以不传这个属性；后端按 `Auto` 处理：

```json
{
  "OpenProfileMode": {
    "PropertyType": "App::PropertyEnumeration",
    "value": "Auto"
  }
}
```

支持值：

| 值                   | 含义                                                                                                    |
| -------------------- | ------------------------------------------------------------------------------------------------------- |
| `Auto`             | 默认。closed face 走现有 solid Pad / Pocket；open wire 走 surface extrusion，默认不参与 Body solid fuse / cut。 |
| `Reject`           | 严格兼容 FreeCAD native。open wire profile 返回`open_profile`，不生成 Pad / Pocket shape。             |
| `SurfaceExtrusion` | 显式指定 open wire 拉伸成 face / shell / compound。结果可显示和拾取，但默认不参与 Body solid fuse / cut。 |
| `ThinSolid`        | 后续扩展。Pad open wire 先按厚度生成薄实体，再参与加料。                                                |
| `ThinCut`          | 后续扩展。Pocket open wire 先按厚度生成切削实体，再参与减料。                                           |
| `SurfaceSplitCut`  | 后续扩展。Pocket open wire 生成切割面并按明确 split policy 修改 Body。                                  |

open wire 选择应使用稳定 sketch geometry identity，而不是一次性拓扑枚举名：

```json
{
  "Profile": {
    "PropertyType": "App::PropertyLinkSubList",
    "SubSet": [
      {
        "value": "Sketch",
        "StableSubList": ["g101", "g102", "g103"]
      }
    ]
  }
}
```

上面这个 payload 不传 `OpenProfileMode`，也应该在 `cad-core` 默认路径里成功拉伸 open wire。只有调用方明确需要 FreeCAD native 严格行为时，才传 `OpenProfileMode = Reject`。

兼容期可以接受 `SubList: ["Edge1"]`，但必须诊断它不是稳定 open-wire 引用；正式路径应保存 `StableSubList: ["g<ID>"]`。

thin 模式需要额外属性，不能靠默认值猜：

```json
{
  "OpenProfileMode": {
    "PropertyType": "App::PropertyEnumeration",
    "value": "ThinCut"
  },
  "OpenProfileThickness": {
    "PropertyType": "App::PropertyLength",
    "value": 1.5
  },
  "OpenProfileSide": {
    "PropertyType": "App::PropertyEnumeration",
    "value": "Both"
  }
}
```

`OpenProfileSide` 建议值：`Left`、`Right`、`Both`。没有 thickness 时，`ThinSolid` / `ThinCut` 必须失败，不得自动给厚度。

## 输出合同

open wire 扩展成功时，结果必须明确说明这是扩展语义：

```json
{
  "status": "ok",
  "profileKind": "open_wire",
  "openProfileMode": "Auto",
  "resolvedOpenProfileMode": "SurfaceExtrusion",
  "bodyParticipation": "display_only",
  "sourceProfile": {
    "object": "Sketch",
    "stableSubnames": ["g101", "g102", "g103"]
  }
}
```

`bodyParticipation` 建议值：

| 值               | 含义                                                                           |
| ---------------- | ------------------------------------------------------------------------------ |
| `solid_add`    | 闭合 profile 或 thin solid 已生成实体，Pad 可参与 Body fuse。                  |
| `solid_cut`    | 闭合 profile 或 thin cut 已生成实体，Pocket 可参与 Body cut。                  |
| `display_only` | open wire surface / shell 只作为该 feature 的几何结果发布，不修改 Body solid。 |
| `unsupported`  | 请求语义没有足够信息，例如 Pocket open wire 未提供 thin-cut 或 split policy。  |

诊断 code 要稳定：

| code                                   | 场景                                                           |
| -------------------------------------- | -------------------------------------------------------------- |
| `open_profile`                       | 显式 `Reject` 或 FreeCAD strict mode 下遇到 open wire。       |
| `open_profile_surface_display_only`  | 默认 open wire surface extrusion 成功，但没有修改 Body solid。 |
| `unsupported_open_profile_body_fuse` | open wire 只生成 surface / shell，却被要求参与 Body solid fuse。 |
| `unsupported_open_profile_pocket`    | Pocket open wire 被要求实体减料，但没有 thin-cut 或 split policy。 |
| `missing_open_profile_thickness`     | `ThinSolid` / `ThinCut` 缺少厚度。                         |
| `ambiguous_open_profile_reference`   | `StableSubList` 无法唯一解析到当前 sketch open edge / wire。 |

## 模块落点

### 1. `document`

读取 `OpenProfileMode`、`OpenProfileThickness`、`OpenProfileSide` 等扩展属性。document 层只做类型解析和基础 diagnostics，不判断 Pad / Pocket 业务含义。

落点：

- `cad-core/include/cad_core/document`
- `cad-core/src/document`

### 2. `sketcher`

保持 FreeCAD-style 语义：open-only sketch 的 `InternalShape` 可以为空，但 raw sketch `Shape` 的 edge / wire 必须可发布、可拾取、可追溯。

必须复用 open wire 线段可追溯方案中的 `g<ID>` 身份账本。Pad / Pocket 解析 open wire 时，应通过 `StableSubList` 找到当前请求里的 raw sketch edge / wire，不得靠 `EdgeN` 顺序或 `OpenWireN` display name 持久化。

落点：

- `cad-core/src/sketcher/sketch_internal_result.cpp`
- 后续 `sketch_edge_identity` / raw edge identity ledger 模块

### 3. `part_design/profile_resolver`

把 profile 解析结果从“只能返回 face”扩展成显式 tagged result：

```cpp
enum class ProfileKind {
    ClosedFace,
    OpenWire,
    EdgeCompound
};
```

resolver 只负责解析 profile 和 normal，不负责造 prism、不负责 Body fuse / cut。

要求：

- `Reject` 模式下，open wire 继续返回 `open_profile`。
- `Auto` / `SurfaceExtrusion` 模式下，允许返回 `OpenWire`。
- 多个 `StableSubList` item 可以组合成一个 wire / compound，但必须保留每条 edge 的 source stable identity。
- profile 跨多个对象时，短期先返回 `unsupported_open_profile_multi_target`，除非完整 placement 和 identity 账本已经补齐。

落点：

- `cad-core/src/part_design/profile_resolver.cpp`
- `cad-core/src/part_design/feature_extrude.cpp`

### 4. `part/part_extrusion`

不要在 Pad / Pocket executor 里临时复制 Part::Extrusion 代码。应从 `executePartExtrusion()` 里抽出可复用的底层 helper，例如：

```cpp
PartLinearExtrusionResult buildLinearExtrusionFromProfile(
    const TopoDS_Shape& profile,
    const PartLinearExtrusionOptions& options,
    const part::NamedShape* sourceNamedShape
);
```

Pad / Pocket open wire 路径调用这个 helper。`Part::Extrusion` executor 本身仍然只负责 `Part::Extrusion` 对象。

落点：

- `cad-core/include/cad_core/part/part_extrusion.h`
- `cad-core/src/part/part_extrusion.cpp`

### 5. `part_design/feature_extrude`

把主路径拆清楚：

1. `ClosedFace`：保持现有 Pad / Pocket solid prism、taper、UpTo、Body fuse / cut 逻辑。
2. `OpenWire + Auto`：默认解析成 `SurfaceExtrusion`，调用 Part linear extrusion helper，生成 surface / shell / compound，默认 `bodyParticipation = display_only`。
3. `OpenWire + ThinSolid`：先用厚度生成闭合 strip / solid，再进入 Pad additive solid 路径。
4. `OpenWire + ThinCut`：先用厚度生成 cutting solid，再进入 Pocket subtractive solid 路径。
5. `OpenWire + SurfaceSplitCut`：后续单独定义 split policy，不和普通 Pocket cut 混写。

落点：

- `cad-core/src/part_design/feature_extrude.cpp`
- `cad-core/src/part_design/feature_pad.cpp`
- `cad-core/src/part_design/feature_pocket.cpp`

### 6. `part_design/body`

Body 不能把 non-solid open-wire extrusion 当成 solid Tip 混入实体链。

要求：

- standalone Pad / Pocket 可以返回 open-wire extrusion shape。
- Body 内部 `Auto` / `SurfaceExtrusion` 结果默认只发布 feature 自身结果，并标记 `display_only`。这不是 hard error，但应返回 `open_profile_surface_display_only` 这类 info / warning 诊断，提醒调用方 Body solid 未被修改。
- 只有 `ThinSolid` / `ThinCut` 生成可布尔的 solid 后，才允许更新 Body Tip solid。

落点：

- `cad-core/src/part_design/body.cpp`

### 7. `topo`

open wire 输出必须接入正式 topo 账本：

- source stable identity：`g<ID>`。
- 本次输出拾取名：`EdgeN` / `FaceN` / `WireN`。
- history：从 source open edge 到 extrusion side face / boundary edge 的映射。
- `ElementMap`：发布可用于下游引用恢复的 stableSubname，不用输出排序猜。

短期可先发布 source identity 和 display subshape；但任何 fixture 期望都必须标明 topo history 仍是 pending，不得把 `OpenWireN` 当成长期 stable contract。

落点：

- `cad-core/include/cad_core/topo`
- `cad-core/src/topo`
- WireJoiner / MapperHistory 后续消费路径

### 8. `adapters`

CLI / C ABI / collector adapter 只做协议转换：

- 透传 `OpenProfileMode` 等属性。
- `OpenProfileMode` 缺省时不补写 `Reject`；adapter 必须让核心库按默认 `Auto` 解析。
- 不在 adapter 中把 Pad / Pocket 改写成 Part::Extrusion。
- 不在 collector 中把 cad-core extension expected 标成 FreeCAD native expected。

落点：

- `cad-core/src/adapters`
- `cad-core/tools/collect_freecad_expected.py`

## Pad / Pocket 语义矩阵

| Feature | Profile     | Mode                 | 结果                                                                                       |
| ------- | ----------- | -------------------- | ------------------------------------------------------------------------------------------ |
| Pad     | closed face | `Auto` 或缺省      | 当前 FreeCAD-compatible solid add。                                                        |
| Pad     | open wire   | `Auto` 或缺省      | 默认生成 surface / shell / compound；standalone 可显示，Body 内默认`display_only`。       |
| Pad     | open wire   | `Reject`           | 严格兼容 FreeCAD native，返回`open_profile`，不生成 shape。                               |
| Pad     | open wire   | `SurfaceExtrusion` | 生成 surface / shell / compound；standalone 可显示，Body 内默认`display_only`。          |
| Pad     | open wire   | `ThinSolid`        | 生成薄实体后参与 additive fuse。                                                           |
| Pocket  | closed face | `Auto` 或缺省      | 当前 FreeCAD-compatible subtractive cut。                                                  |
| Pocket  | open wire   | `Auto` 或缺省      | 默认生成 surface / shell / compound；standalone 可显示，Body 内默认`display_only`。       |
| Pocket  | open wire   | `Reject`           | 严格兼容 FreeCAD native，返回`open_profile`，不生成 shape。                               |
| Pocket  | open wire   | `SurfaceExtrusion` | 只生成切割面候选，不默认减料；Body 内默认`display_only`。                                |
| Pocket  | open wire   | `ThinCut`          | 生成切削实体后参与 subtractive cut。                                                       |
| Pocket  | open wire   | `SurfaceSplitCut`  | 后续 split/slice 语义，需单独 expected 和 topo history。                                   |

## expected / oracle 策略

1. FreeCAD native expected 继续记录 FreeCAD 原生行为：Pad / Pocket open wire 不成功，不能生成 native `Pad.Shape` / `Pocket.Shape`。
2. `cad-core` 默认 open wire 成功属于 extension expected。即使请求里没有 `OpenProfileMode`，只要 default `Auto` 生成了 surface extrusion，也必须标成 `cadcore-extension` 或 `cadcore-default-extension`，不能放进 `.freecad.json` 伪装成 FreeCADCmd 直接采集结果。
3. 如果需要几何 oracle，可以用 `Part::Extrusion` 作为 proxy oracle，但文件名和 metadata 必须写清楚 `oracleKind = "part_extrusion_proxy"`。
4. collector 只负责采 FreeCAD native 语义；extension expected 应由 cad-core test fixture 或专门 extension oracle 生成。
5. 每个 extension fixture 至少覆盖：
   - 默认 `Auto` 下 Pad open wire 成功。
   - 默认 `Auto` 下 Pocket open wire 成功生成 feature shape。
   - 显式 `Reject` 下 Pad / Pocket open wire 仍返回 `open_profile`。
   - Pad in Body 默认 open wire 不偷偷 fuse。
   - Pocket in Body 默认 open wire 不偷偷 cut。
   - Pocket `ThinCut` 有 thickness 后才允许 subtractive path。

建议文件命名：

```text
cad-core/fixtures/p5/expected/pad-open-wire-profile.freecad.json
cad-core/fixtures/p5/expected/pad-open-wire-default-surface.cadcore-extension.json
cad-core/fixtures/p5/expected/pocket-open-wire-default-surface.cadcore-extension.json
cad-core/fixtures/p5/expected/pad-open-wire-reject.freecad-aligned.json
cad-core/fixtures/p5/expected/pocket-open-wire-thin-cut.cadcore-extension.json
```

## 实施批次

### S1：默认 `Auto` 支持和 strict opt-out

目标：Pad / Pocket open wire 默认成功，同时仍能显式走 FreeCAD-compatible 拒绝路径。

改动：

- document 解析 `OpenProfileMode`，缺省值为 `Auto`。
- Pad / Pocket 缺省遇到 open wire 返回 `OpenWire` profile result。
- 显式 `OpenProfileMode = Reject` 时返回 `open_profile`。
- tests 锁住两类 expected：`.freecad.json` 仍是 native 拒绝；`.cadcore-extension.json` 才是默认成功。

### S2：Pad / Pocket standalone `SurfaceExtrusion`

目标：open wire Pad / Pocket 都可以默认“拉伸出来”，但不伪装成 Body solid。

改动：

- profile resolver 返回 `OpenWire` tagged result。
- `part_extrusion` 抽出 reusable linear extrusion helper。
- Pad / Pocket open wire 调 helper 生成 face / shell / compound。
- response 标记 `profileKind=open_wire`、`bodyParticipation=display_only`。

### S3：Body 内 open wire participation 策略

目标：Body Tip 不被 non-solid 污染。

改动：

- Body 遇到 Pad `Auto` / `SurfaceExtrusion` 时不执行 solid fuse。
- Body 遇到 Pocket `Auto` / `SurfaceExtrusion` 时不执行 solid cut。
- 输出 feature 自身结果和 `display_only` metadata。
- 若用户要求 Body solid participation，则返回 `unsupported_open_profile_body_fuse` 或 `unsupported_open_profile_pocket`。

### S4：ThinSolid / ThinCut

目标：让 open wire 真正成为 PartDesign 加料 / 减料。

改动：

- 根据 sketch plane、profile normal、`OpenProfileThickness`、`OpenProfileSide` 构造 strip face / cutting solid。
- Pad `ThinSolid` 进入 additive fuse。
- Pocket `ThinCut` 进入 subtractive cut。
- topo history 从 source `g<ID>` 传播到 offset edge、side face、cap face 和 prism result。

### S5：Pocket `SurfaceSplitCut`

目标：如产品确实需要“用一张拉伸面切开实体”，再单独实现 split/slice 语义。

改动：

- 明确定义保留哪一侧、是否自动延伸切割面、是否允许多 body / 多 solid。
- 接入 OCCT split / section history。
- 单独 expected，不和普通 Pocket cut 复用同一个诊断合同。

## 验收命令

本方案文档本身只需检查文本：

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/要求/7-1-12-21-PadPocket-open-wire拉伸扩展要求.md
```

后续实现短跑建议：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests/test_mvp.py
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pad_open_wire_default_surface_extrusion
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pocket_open_wire_default_surface_extrusion
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pad_open_wire_reject_mode_reports_open_profile
```

新增 extension tests 后，再补对应 focused tests：

```bash
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pad_open_wire_body_display_only
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pocket_open_wire_body_display_only
python3 -m unittest tests.test_p5_sketch.CadCoreP5SketchTest.test_p5_pocket_open_wire_thin_cut_requires_thickness
```
