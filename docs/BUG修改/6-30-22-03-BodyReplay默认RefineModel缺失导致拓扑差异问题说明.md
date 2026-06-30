# Body replay 默认 RefineModel 缺失导致拓扑差异问题说明

## 当前结论

这次 `FreeCADCmd` 与 `cad-core` 的结果不一致，根因不是 `FreeCADCmd` oracle 错，也不是最终 `FilletPreview` 选边错误，而是：

`cad-core` 在 `PartDesign::Body` replay additive feature 时，最终 Body 结果没有按 FreeCAD 的 PartDesign 默认 `RefineModel=true` 语义执行 refine。

表现为：

- 几何体积基本一致。
- bbox 基本一致。
- CAD 拓扑数量不同，cad-core 多保留了冗余分割面和边。

原始 case：

- 输入：`/Users/li/Chili3DProject/cad-web-background/temp/input/20-52-20-01.json`
- cad-core 输出：`/Users/li/Chili3DProject/cad-web-background/temp/output/20-52-20-01.json`
- 对比脚本：`/Users/li/Chili3DProject/cad-web-background/cad-core/tools/compare_recompute_with_freecadcmd.py`
- 目标对象：`Pad2Body`

真实 Shape 层面的差异：

```text
cad-core Shape:
  faces=15, edges=30, vertices=15
  volume=660011409.6199924

FreeCADCmd Shape:
  faces=13, edges=27, vertices=15
  volume=659979037.6049099
```

其中体积差在当前容差内，不是主要问题；主要问题是拓扑分割数量不一致。

## 排除项

### 1. 不是 mesh 体积导致的真实几何差异

旧版对比脚本曾把 `results[].mesh` 的三角网格积分体积拿来和 FreeCAD 的 `Shape.Volume` 比较，得到过明显偏小的 cad-core 体积。

这只是比较口径错误。改为 cad-core `--export-object ... --export-format brep` 后，再用 FreeCADCmd 读取 BREP 统计真实 Shape，体积已经接近 FreeCAD oracle。

因此后续判断应基于 BREP Shape summary，而不是渲染 mesh volume。

### 2. 不是 `StableSubList` / `SubList` 优先级直接导致

这个输入中 `FilletPreview.Base` 有：

```json
{
  "value": "Pad2",
  "SubList": ["Edge9"],
  "StableSubList": ["Edge7"]
}
```

这确实容易误导排查方向。但分段验证表明，拓扑差异第一次出现时，Body Tip 还没有到 `FilletPreview`，而是在 `Tip=Pad2` 阶段已经出现。

所以 `FilletPreview` 只是继承了已经偏掉的 Body 拓扑，不是首个根因。

### 3. 不是 `Revolution` 阶段造成

把同一个 Body 的 Tip 逐步设置到不同特征后对比：

```text
Tip=Pad          match
Tip=Revolution   match
Tip=Pad2         different
Tip=FilletPreview different
```

首次偏差发生在 `Pad2` 作为 additive feature 融入 Body 后。

## 最小定位过程

### 分段对比 Body Tip

用同一个输入，只修改 `Pad2Body.Properties.Tip.value`：

```text
Tip=Pad
  cad-core:   faces=6,  edges=12, vertices=8
  FreeCADCmd: faces=6,  edges=12, vertices=8
  status=match

Tip=Revolution
  cad-core:   faces=11, edges=21, vertices=10
  FreeCADCmd: faces=11, edges=21, vertices=10
  status=match

Tip=Pad2
  cad-core:   faces=14, edges=27, vertices=13
  FreeCADCmd: faces=12, edges=24, vertices=13
  status=different

Tip=FilletPreview
  cad-core:   faces=15, edges=30, vertices=15
  FreeCADCmd: faces=13, edges=27, vertices=15
  status=different
```

这说明差异不是最终 fillet 首次引入的，而是 `Pad2` 融入 Body 后已经多出 `2` 个 face 和 `3` 条 edge。

### 单变量验证

只给输入中的 `Pad2` 显式增加：

```json
"Refine": true
```

再跑同样对比：

```text
Tip=Pad2
  cad-core:   faces=12, edges=24, vertices=13
  FreeCADCmd: faces=12, edges=24, vertices=13
  status=match

Tip=FilletPreview
  cad-core:   faces=13, edges=27, vertices=15
  FreeCADCmd: faces=13, edges=27, vertices=15
  status=match
```

这把根因收敛到 `Pad2` 的默认 refine 语义。

## FreeCAD 语义依据

FreeCAD 的 PartDesign feature 默认 refine 来源：

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/Feature.cpp::getPDRefineModelParameter()`
  - 读取 `BaseApp/Preferences/Mod/PartDesign/RefineModel`
  - 默认值是 `true`

```cpp
return hGrp->GetBool("RefineModel", true);
```

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp::FeatureRefine::FeatureRefine()`
  - 构造时把 `Refine` 设置为该默认值

```cpp
this->Refine.setValue(hGrp->GetBool("RefineModel", true));
```

- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureRefine.cpp::FeatureRefine::refineShapeIfActive()`

  - `Refine=true` 时执行 `shape.makeElementRefine()`
  - `Refine=false` 时返回原 shape
- `/Users/li/Chili3DProject/FreeCAD/src/Mod/PartDesign/App/FeatureAddSub.cpp`

  - `FeatureAddSub` 继承 `FeatureRefine`
  - `Pad` / `Pocket` 这类 additive/subtractive feature 因此会继承默认 `RefineModel=true`

因此请求里没有显式写 `Refine` 时，FreeCAD 对 `PartDesign::Pad` 的行为不是 false，而是取 PartDesign preference 默认 true。

## cad-core 当前问题点

当前 `cad-core` 已有两套 refine 读取逻辑：

### 正确的 PartDesign 默认逻辑

`/Users/li/Chili3DProject/cad-web-background/cad-core/src/runtime/feature_executor.cpp`

```cpp
bool readPartDesignFeatureRefine(const app::DocumentObject& object)
{
    const auto refine = app::readBool(object, "Refine");
    if (refine) {
        return *refine;
    }
    return true;
}
```

这和 FreeCAD 默认语义一致：未显式写 `Refine` 时按 true。

### Body replay 当前使用的通用逻辑

`/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/body.cpp::applyFinalResultRefineForFeature()`

当前调用：

```cpp
const auto refined = runtime::applyRefinePropertyForOwner(
    *documentIt->second,
    bodyObject.name,
    context,
    *bodyShape,
    bodyNamedShape
);
```

而 `applyRefinePropertyForOwner()` 的通用版本内部是：

```cpp
const auto refine = app::readBool(propertyObject, "Refine");
return applyRefinePropertyForOwner(..., refine.value_or(false));
```

也就是说，Body replay 在 feature 没有显式 `Refine` 字段时，把 refine 当成 false。

这和 FreeCAD PartDesign 默认 `RefineModel=true` 不一致，导致 Body fuse 后保留了冗余分割拓扑。

## 推荐修复

修复点应在 `cad-core`，不是 FreeCAD oracle，也不是前端。

目标文件：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/part_design/body.cpp`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/src/runtime/feature_executor.cpp`

建议把 `applyFinalResultRefineForFeature()` 中的通用 refine 调用改成 PartDesign 默认语义：

```cpp
const auto refined = runtime::applyPartDesignFeatureRefinePropertyForOwner(
    *documentIt->second,
    bodyObject.name,
    context,
    *bodyShape,
    bodyNamedShape
);
```

或者实现等价逻辑：

```text
如果 feature 是 PartDesign::FeatureRefine / FeatureAddSub / DressUp / Transform 等 PartDesign feature：
  未显式写 Refine 时按 true
否则：
  保持通用 Refine 缺省 false
```

本 case 中 `Pad2` 是 `PartDesign::Pad`，属于 `FeatureAddSub`，应该走 PartDesign 默认 true。

## 为什么不应该在前端修

前端看到的是最终 Body result 的 subshape 和后续引用 DTO，它不应该负责判断：

- FreeCAD PartDesign preference 默认值；
- Body replay 中每一步是否应该 refine；
- fuse 后的冗余 face/edge 是否应被合并；
- `BRepBuilderAPI_RefineModel` 的拓扑历史如何传播。

这些都属于后端 CAD kernel / PartDesign 语义。前端只能消费 `/cad/recompute` 返回的结果和 element reference update，不能用 mesh 或 face 编号猜 refine 行为。

## 为什么不应该改 FreeCAD oracle

FreeCADCmd oracle 当前表现符合 FreeCAD 源码：

- `PartDesign::FeatureRefine` 默认读取 `RefineModel=true`。
- `Pad2` 未显式写 `Refine` 时，native FreeCAD 仍按默认 true refine。
- 显式给 cad-core 输入补 `Pad2.Refine=true` 后，cad-core 与 FreeCADCmd 拓扑数量立刻收敛。

因此 oracle 是有效信号，问题在 cad-core 对默认 `Refine` 的解释。

## 验收标准

### 1. 原始 case 收敛

命令：

```bash
cd /Users/li/Chili3DProject/cad-web-background

python3 cad-core/tools/compare_recompute_with_freecadcmd.py \
  temp/input/20-52-20-01.json \
  --output temp/output/20-52-20-01.json \
  --native-out temp/compare/20-52-20-01.freecad.json \
  --report temp/compare/20-52-20-01.compare.json
```

修复前：

```text
status=different
topology_counts:
  cad-core Shape: faces=15, edges=30, vertices=15
  FreeCADCmd:     faces=13, edges=27, vertices=15
```

修复后期望：

```text
status=match
topology_counts:
  cad-core Shape: faces=13, edges=27, vertices=15
  FreeCADCmd:     faces=13, edges=27, vertices=15
```

### 2. 分段 Body Tip 验证

对同一输入，把 `Pad2Body.Properties.Tip.value` 分别改成：

- `Pad`
- `Revolution`
- `Pad2`
- `FilletPreview`

期望全部匹配 FreeCADCmd。尤其是：

```text
Tip=Pad2
  cad-core:   faces=12, edges=24, vertices=13
  FreeCADCmd: faces=12, edges=24, vertices=13

Tip=FilletPreview
  cad-core:   faces=13, edges=27, vertices=15
  FreeCADCmd: faces=13, edges=27, vertices=15
```

### 3. 显式 Refine=false 不能被强行 refine

需要补一个反向测试：

```json
"Pad2": {
  "Properties": {
    "Refine": false
  }
}
```

期望 cad-core 保持不 refine，并与 FreeCADCmd 显式 `Refine=false` 行为一致。

这个测试很重要，避免把修复写成“所有 Body replay 都无条件 refine”。

## 回归测试建议

建议新增或扩展 cad-core fixture：

1. `PartDesign::Pad` 未显式 `Refine`，在 Body 中 replay 后应按默认 true refine。
2. `PartDesign::Pad` 显式 `Refine=false`，Body replay 不应 refine。
3. `PartDesign::Fillet` 作为 Tip 时，不应掩盖前一步 Pad fuse refine 的差异。

候选测试文件：

- `/Users/li/Chili3DProject/cad-web-background/cad-core/tests/test_p6_topology.py`
- `/Users/li/Chili3DProject/cad-web-background/cad-core/tests/test_p7_features.py`

如果已有 Body replay / PartDesign refine 测试，优先在原测试组补断言，不另开大范围 fixture。

## 非目标

- 不修改 `FreeCADCmd` oracle 采集逻辑。
- 不在前端按 face/edge 数量或 mesh 数据修正结果。
- 不在 compare 脚本里隐藏真实拓扑差异。
- 不把所有 feature 都默认 refine；仅 PartDesign `FeatureRefine` 语义下的 feature 未显式写 `Refine` 时应默认 true。
- 不把 `StableSubList` / `SubList` 优先级问题作为本 bug 的主因。

## 一句话总结

`Pad2` 没写 `Refine` 时，FreeCAD 按 PartDesign 默认 `RefineModel=true` 清理冗余拓扑；cad-core Body replay 当前按 false 处理，导致 fuse 后多出冗余 face/edge。显式给 `Pad2.Refine=true` 后差异消失，因此修复应让 Body replay 使用 PartDesign 默认 refine 语义。
