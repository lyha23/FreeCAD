# CAD Core MVP 执行总览

本目录是 `../00-CAD-Core抽取方案.md` 的可执行拆解。当前目标不是一次抽完整 FreeCAD，而是先搭一个无 Qt、无 Web、可独立调用的 CAD Core MVP。

## 当前实现状态

当前 `/cad-core` 已经落地为独立 C++/CMake MVP，并真实接入 OCCT。`cad-core-lib` 是 Core 库，`cad-core` 是 CLI adapter，`cad_core_ffi` 是薄 C ABI adapter。

已验证的边界：

- 输入采用 `Objects[]` / `Name` / `ID` / `TypeId` / `Properties`。
- registry 注册 `Sketcher::SketchObject`、`PartDesign::Body`、`PartDesign::FeatureBase`、`PartDesign::Pad`、`PartDesign::Pocket`。
- `rect-pad.json` 能生成 OCCT mesh、bbox、volume、subshape map。
- diagnostics fixtures 覆盖未知类型、重复对象、缺失链接、开口草图、不支持参数和非法长度。
- `PropertyLink` / `PropertyLinkList` / `PropertyLinkSub` / `PropertyLinkSubList` 已经归一成 recompute dependency edge。
- P2 已有 `FeatureBase` / `FeatureAddSub` / `FeatureExtrude` 最小主链：Pad 写 `add_shape`，Pocket 写 `sub_shape`，Body 按顺序 Fuse / Cut。
- `rect-pad-pocket.json` 能生成 Pocket 后的 Body mesh、bbox、volume、subshape map。

未实现的边界：

- 完整 FreeCAD `FeatureExtrude`：`ThroughAll`、`UpToFace`、`UpToShape`、`TwoLengths`、taper、custom direction、attachment/support/subname 恢复。
- 完整 Body 历史/placement/refine/suppressed 语义。
- Pattern / Mirror / MultiTransform、Hole、Fillet、Chamfer、完整 topo naming 更新。

## MVP 目标

第一版只证明一条闭环：

```text
FreeCAD 风格 `DocumentObject graph`
  -> cad-core CLI
  -> recompute
  -> Sketch + Body + Pad
  -> mesh / subshape map / diagnostics
```

完成后应能用一份输入文件生成 Pad 的可检查结果，并且未知类型、缺失链接、执行失败都能返回明确 diagnostics。

## 交付边界

必须做：

- FreeCAD 风格 `Objects[]` / `DocumentObject` / `Properties` 模型。
- `Diagnostics`、`FeatureRegistry`、一次性 `ComputeContext`。
- 无 Qt / 无 Web 的 `cad-core` 命令行入口。
- 最小依赖图和 recompute 顺序。
- `Sketcher::SketchObject` 最小执行。
- `PartDesign::Body` 最小特征链。
- `PartDesign::Pad` 长度拉伸。
- Pad mesh 导出。
- Pad subshape map 导出。
- 最小 fixtures 和 FreeCAD 对照结果。

MVP 暂不做：

- Web / WASM / Worker adapter 的产品化接入。C ABI 可以保留为薄 adapter，但不能反向影响 Core 边界。
- Hole、Fillet、Chamfer、Pattern、Mirror、Scaled。
- 完整 topo naming 稳定更新。
- 完整 Sketcher 约束求解器。
- 长生命周期 shape cache。
- GUI 选择、ViewProvider、TaskPanel、Workbench。

## 执行顺序

按下面顺序推进，不要先接 Web 或产品化 adapter：

| 步骤 | 文档 | 目标 |
| --- | --- | --- |
| 1 | `01-P0-Core壳.md` | 固定 MVP 输入输出模型。 |
| 2 | `01-P0-Core壳.md` | 建无 Qt 的 Core 目标和 CLI。 |
| 3 | `01-P0-Core壳.md` | 实现 diagnostics、registry、空 recompute。 |
| 4 | `01-P0-Core壳.md` | 加最小依赖图和 `recompute.objs` 目标计算。 |
| 5 | `02-P1-Sketch-Body-Pad闭环.md` | 实现 Sketch 最小 profile。 |
| 6 | `02-P1-Sketch-Body-Pad闭环.md` | 实现 Body 顺序和 Tip。 |
| 7 | `02-P1-Sketch-Body-Pad闭环.md` | 实现 Pad 长度拉伸。 |
| 8 | `02-P1-Sketch-Body-Pad闭环.md` | 导出 mesh 和 subshape map。 |
| 9 | `03-接口与验收样例.md` | 用 fixtures 固化验收。 |

P0 + P1 已落地，P2 的 `FeatureBase` / `FeatureAddSub` / `FeatureExtrude` / Pocket Length 最小链也已落地。后续继续扩 `FeatureExtrude` 和 Body 语义时，仍不要绕过 `FeatureBase` / `FeatureAddSub` 直接在单个 feature 里做最终实体修补。

下一阶段执行 `05-P3a-FeatureExtrude-UpTo终止语义.md`：

```text
ThroughAll
  -> FaceN subshape 解析
  -> UpToFace
  -> UpToShape
  -> Pad / Pocket UpTo fixtures
```

Hole、Fillet、Chamfer、Pattern、Mirror 继续后置，等 `FeatureExtrude` 的终止语义和 subshape 引用稳定后再进入。

## 完成定义

MVP 完成需要同时满足：

- `cad-core recompute fixtures/mvp/rect-pad.json --output out/rect-pad.result.json` 能成功运行。
- 输出里有 `Pad` 的 mesh 摘要或 mesh 文件路径。
- 输出里有 `Pad` 的 subshape map。
- 正常 case 的 diagnostics 为空。
- 未知 `TypeId` 返回 diagnostics，不生成假成功结果。
- 缺失 `Profile` 链接返回 diagnostics，不崩溃。
- 开口草图返回 diagnostics，不生成 Pad。
- 同一个 fixture 的关键结果能和 FreeCAD 对照文件比较。
- P2 的 `rect-pad-pocket.json` 能返回 Body 的 cut 后 bbox、volume、mesh 和 subshape map，且 diagnostics 为空。
- P2 错误 fixture 的 diagnostics code 稳定。

## 推进原则

- 每个 executor 只支持白名单参数；不支持的参数要诊断，不能静默忽略。
- `ComputeContext` 只活在一次 recompute 内，不保存跨请求裸 shape。
- 文档模型不直接暴露 FreeCAD C++ 对象，也不暴露前端状态树。
- 持久输入只保存 `Objects[]`；`recompute.objs` 只是本次运行目标，不进入持久层。
- 每完成一个能力，至少补一个最小 fixture。
- 文档只记录当前可执行结论，不记录过程流水账。
