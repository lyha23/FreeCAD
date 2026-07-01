# display-only surface Body Topo History 累计显示修复方案

## 背景

当前 `cad-core` 已经支持两条 open wire surface extrusion 路径：

1. 从 raw Sketch edge 拉出 display-only surface。
2. 从已有 surface / solid 的 `EdgeN` 继续拉出 display-only surface。

单次拉伸是正确的。问题出在连续 open wire surface Pad 串联后，`PartDesign::Body` 只显示 Tip feature 的 surface，前面已经存在的 display-only surface 会从 Body 结果中消失。

典型请求链路：

1. `Sketch` 只有一条 `LineSegment`。
2. `Pad` 以 `OpenProfileMode=SurfaceExtrusion` 把线段拉成第一张面，范围 `z=0 -> z=375`。
3. `Pad2` 以 `Pad.Edge4` 为 profile 继续拉成第二张面，范围 `z=375 -> z=614`。
4. `Pad2Body.Group = [Sketch, Pad, Pad2]`，`Tip = Pad2`。

当前输出只包含第二张面，也就是 `z=375 -> z=614`。如果产品语义是“连续线段拉面形成一组可显示 surface”，Body 应该显示两张面组成的 history-backed compound，而不是只显示 Tip。

## 现有依据

已有要求文档已经明确：

- `Pad/Pocket open wire 拉伸扩展要求`：open wire surface extrusion 是 `cad-core` 产品扩展，默认 `display_only`，可显示、可拾取、可追溯，但不参与 Body solid fuse / cut。
- `已有 surface / solid 边继续开放拉伸方案`：从已有 surface / solid 的边继续 open-profile 拉伸已经属于 profile acquisition 能力，不能退回 Sketch-only 解析。
- `CADCore2.0/P5P6-ExternalGeometry-TopoNaming` 主线：MapperHistory 是 `NamedShape` / `ElementMap` 的统一入口；`ElementMap` 只写唯一 target，split / deleted / ambiguous 必须留在 mapper history / terminal history / diagnostics。
- `6-3-Helper输出迁移要求`：不能因为输出数量看起来正确就删除 helper 或拼几何；必须证明 `NamedShape.history`、`ElementMap` 和后续引用恢复能消费同一份 identity / provenance。

本方案只补第三层：`PartDesign::Body` 对多个 `display_only` surface feature 的结果聚合。这里的聚合不能只是裸 `TopoDS_Compound`，而必须由 Topo History / NamedShape 证据驱动，让最终 Body result 既能显示完整 surface 链，也能稳定发布每个子元素的 feature owner。

## 问题定位

当前落点在 `cad-core/src/part_design/body.cpp`：

- `executeBodyTopoShape()` 遍历 `Body.Group`。
- 当 feature 没有 `AddSubShape`，且 `context.objects[feature]["bodyParticipation"] == "display_only"` 时，只把 feature 名写入 `displayOnlyFeatures`。
- 如果该 feature 是 Tip 且当前还没有 `bodyShape`，才把 Tip 的 `context.shapes[feature]` 作为 Body 结果。
- 如果前面已经有 display-only surface，且 Tip 也是 display-only surface，当前逻辑没有把这些 surface 合并成一个 Body 显示结果。

这会导致：

- `display_only_features` 可以记录 `["Pad", "Pad2"]`，但 `Body.mesh` / `Body.subshapes` 只来自 `Pad2`。
- 第一张 surface 的 edge / face 在最终 Body result 中不可见、不可拾取。
- 下游只能看到 Tip 面，不能对连续 surface 链做完整显示和后续选择。
- 即便只把 `Pad` 和 `Pad2` 的 shape 做成 compound，如果没有同步 Topo History / NamedShape owner，最终仍可能把所有 `EdgeN` / `FaceN` 错归到 `Pad2`，后续 profile、StableSubList、ReferenceShadow 仍会追错对象。

## 目标

1. Body 遇到多个 `bodyParticipation=display_only` feature 时，最终显示 shape 应包含这些 display-only feature 的累计结果。
2. 累计结果必须携带 Topo History：每个 compound child 的来源 feature、来源 subshape、生成/保留关系要进入 `NamedShape.mapper_history` 或等价 Body display history ledger。
3. Body result 的 subshape 路径必须保持 feature-qualified：例如 `Pad.Edge4`、`Pad2.Face1`，不得把所有子元素伪装成裸 `EdgeN` / `FaceN`，也不得全部归到 Tip。
4. `ElementMap` 只写唯一可证明的 target；无法证明唯一恢复时，保留 mapper history / diagnostics，不猜唯一 stableSubname。
5. 不改变实体语义：display-only surface 仍然不参与 solid fuse / cut，不生成体积，不影响 `solid_add` / `solid_cut`。
6. 当 Body 只有 display-only surface、没有 solid base 时，Body result 应是 history-backed surface compound / shell 类显示结果，`volume=0`。
7. 当 Body 已经有 solid base，又有 display-only feature 时，短期先不把 surface 混进 solid Body 主 shape，避免改变实体工作流；这种场景应保留 `display_only_features`，必要时作为后续“overlay channel”单独设计。

## 非目标

- 不把 open wire surface 默认转成实体块。
- 不对 display-only surface 做 `fuseShapes()` / `cutShapes()`。
- 不修前端字段写法；`StableSubList: ["Edge4"]` 仍只能作为 current-name fallback，并继续允许 `ambiguous_open_profile_reference` warning。
- 不在 runtime response 层拼接 mesh；聚合应发生在 Body topo shape 层。
- 不通过输出层字符串拼接、mesh 点线重组、按 `EdgeN` 排序或 bbox 猜测来补 owner。
- 不为 ambiguous / split / deleted 子元素伪造唯一 `ElementMap`。
- 不改变 FreeCAD native Pad / Pocket face-first 语义。

## 方案

### 1. 修复主线：Topo History 驱动的 display compound

在 `executeBodyTopoShape()` 中累计 display-only feature 时，同时累计几何和历史证据。几何 compound 只是承载最终显示；真正的 owner / stableSubname 来源必须来自每个 feature 自己的 `NamedShape` / `mapper_history` / `element_map`。

新增局部状态建议：

```cpp
std::vector<std::pair<std::string, TopoDS_Shape>> displayOnlyShapes;
std::vector<std::pair<std::string, part::NamedShape>> displayOnlyNamedShapes;
```

遍历 `Group` 时，如果 feature 是 `display_only` 且存在 `context.shapes[feature]`：

1. 继续记录 `displayOnlyFeatures.push_back(feature)`。
2. 把 feature shape 加入 `displayOnlyShapes`。
3. 如果 `context.namedShapes[feature]` 存在，把 named shape 一起记录；如果不存在，不得用裸 `EdgeN` / `FaceN` 伪造稳定历史。
4. 为 Body display compound 记录 child source：`feature name`、child shape、child named shape、child local subshape namespace。
5. 不进入 solid fuse / cut。

当 `feature == resolvedStopFeature` 时：

- 如果尚无 solid `bodyShape`，用所有已记录 display-only shapes 建一个 history-backed compound 作为 Body 显示 shape。
- 如果已有 solid `bodyShape`，保持现有 solid 结果，不把 display-only surface 混入主 shape；后续另开 overlay channel。

### 2. 新增 display history ledger

不要让 `directTipSubshapeOwner` 继续承担多 feature owner 分派职责。它只能表达“单个 display-only Tip 直接作为 Body 显示”的旧情况。

对多个 display-only feature，建议新增明确结构，例如：

```cpp
struct DisplayOnlyChild {
    std::string feature;
    TopoDS_Shape shape;
    std::optional<part::NamedShape> namedShape;
};
```

`BodyTopoShapeResult` 增加可选字段：

```cpp
std::vector<DisplayOnlyChild> displayOnlyChildren;
bool bodyAdoptedDisplayOnlyCompound = false;
```

后续 `executeBody()` / response-shaping 应按 `displayOnlyChildren` 分派 owner，而不是把 compound 的所有 subshape 都归到 `resolvedStopFeature`。

### 3. compound helper 只负责几何，不负责身份

不要把 compound 构造散落在循环里。建议在 `body.cpp` 内部新增文件私有 helper：

```cpp
TopoDS_Shape compoundDisplayOnlyShapes(
    const std::vector<std::pair<std::string, TopoDS_Shape>>& shapes);
```

实现使用 `BRep_Builder` / `TopoDS_Compound`：

1. 空数组返回 null / `std::nullopt`。
2. 单个 shape 直接返回该 shape，保持现有单次行为。
3. 多个 shape 构造 compound，跳过 null shape。

这个 helper 只返回几何 shape，不写 stableSubname，不构造 `ElementMap`。Topo identity 必须由 `displayOnlyChildren` 里的 child `NamedShape` 和后续 response-shaping 处理。

### 4. NamedShape / subshape owner 策略

Body compound 不能简单套用 Tip 的 `NamedShape`，否则第一张面的 source path 会丢。

必须改成按 child feature 分派：

1. 单个 display-only Tip：沿用现有 `bodyAdoptedDisplayOnlyTip` 逻辑，`directTipSubshapeOwner = resolvedStopFeature`。
2. 多个 display-only feature：设置 `bodyAdoptedDisplayOnlyCompound = true`，并清空或禁用单值 `directTipSubshapeOwner`。
3. 对 compound 结果，不伪造完整 `NamedShape.element_map`；先从 child `NamedShape` 复制可证明的 mapper history / element aliases。
4. 如果 child `NamedShape.element_map` 能唯一证明 `Pad.Edge4 -> Body compound child edge`，可以发布对应 feature-qualified stableSubname。
5. 如果只能证明“这个 child 来自 Pad”，但无法唯一证明具体 edge / face，仍发布当前 `subname` 供本次拾取，但 `stableSubname` 必须为空或降级，并保留 diagnostic / `identityStatus`，不得用 `EdgeN` 伪造稳定身份。
6. 如果当前 `runtime::recompute.cpp` / Body response-shaping 只能处理单一 owner，则本轮必须扩展为按 `displayOnlyChildren` 做 child-source 分派；否则几何 compound 修复不算完成。

验收要求是最终 Body result 中能同时看到：

- 第一张面对应的 `Pad.FaceN` / `Pad.EdgeN`。
- 第二张面对应的 `Pad2.FaceN` / `Pad2.EdgeN`。
- 两组子元素的 `stableSubname` 只能来自各自 child `NamedShape` / mapper history 中可证明的 alias；不能从当前 `indexed` 复制。

### 5. 保持 display-only 诊断语义

`open_profile_surface_display_only` 仍然是 warning，不是 error。

连续两次 open wire surface Pad 时，允许出现两条该 warning：

```text
Pad  -> open_profile_surface_display_only
Pad2 -> open_profile_surface_display_only
```

如果 `Pad2` 使用 `StableSubList: ["Edge4"]` 这类 current-name fallback，继续保留：

```text
ambiguous_open_profile_reference
```

这个 warning 和 Body 聚合修复是两个问题，不能因为它存在就让 Body 丢第一张面。

## 回归 fixture

新增 fixture：

```text
cad-core/fixtures/p7/partdesign-body-display-only-surface-chain.json
```

内容基于当前失败例：

- 一个 open `Sketch` line。
- `Pad` 从 `Sketch.g100001` 拉第一张 surface，`Length=375`。
- `Pad2` 从 `Pad.Edge4` 拉第二张 surface，`Length=239`。
- `Body.Group = [Sketch, Pad, Pad2]`，`Tip=Pad2`。
- `recompute.objs = ["Body"]`。

如果已有 fixture 可复用，也可以在 `test_p7_features.py` 中直接组装 payload，但建议落 fixture，方便 HTTP / FFI / CLI 复用。

## 测试要求

在 `cad-core/tests/test_p7_features.py` 增加测试：

```python
def test_p7_body_accumulates_display_only_surface_chain(self) -> None:
    result = self.run_recompute("partdesign-body-display-only-surface-chain", "p7")
    body = result["objects"]["Body"]
    mesh = result["mesh"]["Body"]
    subshapes = result["subshapes"]["Body"]

    self.assertEqual(body["status"], "ok")
    self.assertEqual(body["display_only_features"], ["Pad", "Pad2"])
    self.assertEqual(body["volume"], 0.0)
    self.assertGreaterEqual(len(mesh["triangles"]), 4)
    self.assertTrue(any(s["subname"].startswith("Pad.") for s in subshapes))
    self.assertTrue(any(s["subname"].startswith("Pad2.") for s in subshapes))
    self.assertFalse(any(
        s.get("stableSubname") == s.get("indexed")
        for s in subshapes
        if s.get("stableSubname")
    ))
```

具体断言建议：

1. diagnostics code 顺序为：
   - `open_profile_surface_display_only`
   - 可选 `ambiguous_open_profile_reference`
   - `open_profile_surface_display_only`
2. Body mesh 顶点或 edgeSegments 同时覆盖 `z=0 -> z=375` 和 `z=375 -> z=614`。
3. Body subshapes 同时包含 `Pad.*` 和 `Pad2.*`。
4. `Pad` / `Pad2` 自身结果仍为 `shape=occt_shell` 或等价 surface 类型，`bodyParticipation=display_only`。
5. `volume=0.0`。
6. Body 或 child `NamedShape` 的 `mapper_history` / `element_map_status` 能说明 `Pad`、`Pad2` 两个 feature 都参与了 display compound；如果没有唯一 `ElementMap`，必须有明确 diagnostic / status，而不是假装稳定。
7. 不允许出现把 `stableSubname` 直接等同当前 `indexed` 的伪稳定写法，例如裸 `Edge4` / `Face1`。

## 实施步骤

1. 先把失败 fixture 加入 `cad-core/fixtures/p7/`，写红测试，证明当前 Body 只显示 `Pad2`。
2. 先设计 `displayOnlyChildren` / display history ledger 的数据结构，确认它能把 feature name、shape、NamedShape evidence 一起带到 response-shaping。
3. 在 `body.cpp` 增加 display-only shape 累计容器和 compound helper；helper 只管几何，不写身份。
4. 修改 display-only 分支：遇到每个 display-only feature 都缓存 shape 和 NamedShape；Tip 到达时，如果没有 solid body，则用缓存 compound 作为 `bodyShape`，同时设置 `bodyAdoptedDisplayOnlyCompound`。
5. 调整 Body result metadata：`display_only_features` 必须完整返回；多 display-only compound 不能把 `direct_tip_subshape_owner` 错设为单个 Tip。
6. 扩展 response-shaping：按 `displayOnlyChildren` 分派 subshape owner，并把可证明的 child `mapper_history` / `ElementMap` alias 传播到 Body result。
7. 如果某个 child 无法证明唯一 stable target，输出当前 `subname` 但降级 `stableSubname` / `identityStatus`，保留 diagnostic，不猜。
8. 跑 focused test，再跑 p7 全量测试。

## 验证命令

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_p7_features.CadCoreP7FeatureTest.test_p7_body_accumulates_display_only_surface_chain
python3 -m unittest tests.test_p7_features
```

如果改动触及 response-shaping 或 adapter 输出，再补跑：

```bash
python3 -m unittest tests.test_adapters
python3 -m unittest tests.test_expected_fixtures
```

## 完成标准

- 连续 open wire surface Pad 的 Body result 显示所有 display-only surface，不只显示 Tip。
- `display_only_features` 完整记录参与显示聚合的 feature 列表。
- Body result 的 `subshapes[]` 同时发布 `Pad.*` 和 `Pad2.*` owner，且 owner 来源来自 `displayOnlyChildren` / child `NamedShape`，不是输出层字符串拼接。
- `NamedShape.mapper_history` / `ElementMap` 规则被尊重：唯一可证明才写 stable alias；ambiguous / split / deleted 只进入 history / diagnostics。
- 普通 closed-profile Pad/Pocket、thin solid/thin cut、dress-up replacement solid 行为不回归。
- 没有把 display-only surface 误并入 Body solid；体积仍为 0。
- 没有新增前端 prefix guess 或 response 层 mesh 拼接。
