# P6：Topo Naming 主路径

P6 的目标是把稳定引用从导出层补丁升级为 CAD Core 的正式账本。完成后 Pad / Pocket / Sketch external reference / DressUp / Pattern 都通过 `NamedShape`、`ElementMap`、MapperHistory 传播稳定 subname，而不是在 executor 或 adapter 中补猜测规则。

## FreeCAD 语义来源

| 语义 | FreeCAD 参考位置 |
| --- | --- |
| Shape 属性 | `src/Mod/Part/App/PropertyTopoShape.cpp` |
| TopoShape | `src/Mod/Part/App/TopoShape.cpp`、`TopoShapeExpansion.cpp` |
| Mapper | `src/Mod/Part/App/TopoShapeMapper.cpp` |
| PartFeature | `src/Mod/Part/App/PartFeature.cpp` |
| GeoFeature 引用更新 | `src/App/GeoFeature.cpp` |
| LinkSub 更新 | `src/App/PropertyLinks.cpp` |
| FaceMaker / WireJoiner history | `src/Mod/Part/App/FaceMaker*.cpp`、`WireJoiner.cpp` |

## 当前问题

- 当前 subshape map 能导出 `FaceN` / `EdgeN` / `VertexN`，但完整 `NamedShape` / `ElementMap` / MapperHistory 还不是主路径。
- P3a 的 UpToFace 解析依赖当前 shape 的 indexed face，后续需要 stable subname 和旧引用恢复。
- Refine、taper、boolean、split、FaceMaker / WireJoiner 都需要正式 history。

## Step 40：NamedShape 核心模型

目标：

- 为每个 feature 输出建立 `NamedShape`。
- 记录 generated / modified / deleted / split / merge 来源。
- 支持 bare indexed subname、stable subname、mapped tail 的解析。

验收：

- `Face1` 可解析，但不等同于稳定 topo naming 完成。
- stable subname 缺失时有 diagnostics 或 known gap，不伪装为恢复成功。

## Step 41：ElementMap 和引用更新

目标：

- 建立 object-local `ElementMap`。
- 支持 `PropertyLinkSub` / `PropertyLinkSubList` 的旧引用更新。
- 支持 internal element 和 external reference。

fixtures：

```text
fixtures/p6/
  pad-length-change-fillet-edge.json
  sketch-external-edge-after-pad-change.json
  pocket-face-reference-after-up-to.json
```

验收：

- 修改 Pad 长度后，引用不无声丢失。
- 无法恢复时返回 diagnostics。

## Step 42：MapperHistory 消费

目标：

- boolean、extrude、taper、refine、pattern 都把 OCCT / geometry history 消费到 topo。
- source edge 一对多 fragment 映射有明确记录。
- split / merge 不能靠几何排序后补。

排查矩阵：

```text
1. FaceMakerBuildFace 几何结果是否与 FreeCAD 一致
2. WireJoiner::getOpenWires 几何结果是否与 FreeCAD 一致
3. raw compound / child shape identity 是否在组合时被重建或复制
4. NamedShape / ElementMap 是否完整消费 MapperHistory
```

如果 1-3 一致而 stable subname 不一致，应归类为 history 到 ElementMap 的传播缺口，不应在 sketch executor 继续加规则。

## Step 43：Refine 和 ShapeFix history

目标：

- Refine 迁移 FreeCAD maker / object chain。
- ShapeFix 的 generated / modified history 进入 ElementMap。
- 删除临时 refine fallback 的输出修正路径。

fixtures：

```text
fixtures/p6/
  refine-pad-pocket.json
  refine-fillet-reference.json
  shapefix-wire-history.json
```

## 完成定义

P6 完成需要同时满足：

- `NamedShape` / `ElementMap` / MapperHistory 是 recompute 主路径。
- LinkSub 旧引用更新可被 fixture 约束。
- Face/Edge/Vertex 数量、几何内容、stable subname 丢失被当作失败。
- 仅命名顺序不同且几何等价时，归类为命名顺序差异。
- executor 和 adapter 中没有 fixture 名称或几何形态猜测的 topo naming 修补逻辑。
