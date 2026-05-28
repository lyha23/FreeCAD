# 12-ShapeBinder和跨Body引用：零件之间怎么借形状

## 一句话结论

ShapeBinder/SubShapeBinder 用来把别的对象或子形状带进当前上下文。它不是普通复制粘贴，而是保存引用，并在重算时根据引用重新生成一个 Shape。

## 用户视角

用户在一个 Body 里想用另一个对象的边、面或整体形状作为参考，可以创建 ShapeBinder 或 SubShapeBinder。之后这个 Binder 可以参与草图、定位或后续特征。

## 对象视角

`ShapeBinder` 的重点属性：

- `Support`：被绑定的对象和子元素。
- `TraceSupport`：是否跟踪支撑对象的位置变化。

`SubShapeBinder` 更复杂：

- `Support`：可以跨文档/跨对象引用子形状。
- `BindMode`：`Synchronized`、`Frozen`、`Detached`。
- `BindCopyOnChange`：是否在源对象变化时复制。

引用图：

```mermaid
flowchart LR
    Source[源对象 Shape] --> Support[Support 链接]
    Support --> Binder[ShapeBinder / SubShapeBinder]
    Binder --> BinderShape[Binder.Shape]
    BinderShape --> Target[当前 Body 后续特征使用]
```

## 几何视角

`ShapeBinder::updatedShape()` 会根据 Support 读取源对象或子形状。`SubShapeBinder::update()` 会把多个引用形状收集起来，必要时变换位置、合成 compound、做面、做 offset 或 refine。

这意味着 Binder 的结果是一个真正的 Shape，可以被后续几何流程使用。

## 重算视角

`ShapeBinder::mustExecute()` 会检查 `Support` 和 `TraceSupport`。`execute()` 会重新设置 Shape。`SubShapeBinder::execute()` 在同步模式下调用 `update()`。

如果源对象变化，SubShapeBinder 还会通过信号和 CopyOnChange 逻辑决定是否同步、复制或变成 mutated 状态。

## 源码索引

- `src/Mod/PartDesign/App/ShapeBinder.h`：`ShapeBinder`、`SubShapeBinder` 属性定义。
- `src/Mod/PartDesign/App/ShapeBinder.cpp`：`ShapeBinder::updatedShape()`、`execute()`、`buildShapeFromReferences()`、`SubShapeBinder::update()`、`SubShapeBinder::execute()`。
- `src/Mod/PartDesign/Gui/TaskShapeBinder.cpp`：选择支撑对象和子元素的任务面板。
- `src/Mod/PartDesign/PartDesignTests/TestShapeBinder.py`：行为测试。

## 常见误区

- Binder 不是一次性复制，它可以同步源对象。
- Frozen/Detached/Synchronized 不是显示选项，而是引用更新方式。
- 跨 Body 引用要谨慎，因为上游拓扑变化会影响下游引用。

