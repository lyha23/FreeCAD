# subname / fullSubname / stableSubname 三层语义说明

这三个字段看起来都像“面名”，但语义不是一回事。最容易出错的地方，就是把当前看到的 `Face11` 当成长期稳定身份。

大白话可以这样记：

- `subname`：当前怎么点到它。
- `fullSubname`：显示路径从哪一路来的。
- `stableSubname`：以后重算后靠什么身份证找回它。

## 三个字段分别干什么

`subname` 是当前引用名。

例如：

```text
value = RevolutionBody
subname = Face11
```

意思是：在当前 `RevolutionBody` 的结果 shape 里找 `Face11`。它解决的是“这一次请求里能不能选中这个子元素”。

`fullSubname` 是完整显示路径。

例如：

```text
fullSubname = RevolutionBody.Revolution.Face11
```

它说明 UI 或 Body 展示链路里，这个元素看起来是沿着 `RevolutionBody -> Revolution -> Face11` 这条路径来的。但它只是显示路径证据，不等于 `Revolution.Shape` 自己真的有 `Face11`。

`stableSubname` 是稳定身份证。

它解决的是：拓扑重建后，`Face11` 可能变成 `Face9`、`Face13`，后续 feature 还想找回“同一个语义上的面”，应该靠什么。

因此它不能随便填当前枚举名 `Face11`。正确来源应该是 FreeCAD / `NamedShape` / `ElementMap` 能证明的稳定身份。例如 FreeCADCmd 通过 `Shape.getElementMappedName("Face5")` 采到的：

```text
Pad.#f:1;FAC;:H293:4,F
```

这串东西人不用理解格式，前端和后端也不应该手工拼。它是 opaque string，只能原样保存、原样提交、原样解析。

## 情况一：面能归到 Tip feature

例如在 `Body` 结果里点到一张面，但这张面在真正的 Tip feature `Pad.Shape` 里也存在：

```text
Body.Pad.Face5
```

这时可以认为：

```text
Body 是展示入口
Pad 是这张面的真实 owner
```

输出应该类似：

```text
subname = Pad.Face5
stableSubname = Pad.#f:1;FAC;:H293:4,F
fullSubname = Body.Pad.Face5
```

这里的 `Pad.` 前缀表示：这个稳定身份属于 `Pad` 的命名空间。后续如果要引用 `Pad` 的这张面，就去 `Pad` 的拓扑命名账本里用 `#f:1;FAC;:H293:4,F` 找。

不要把它简化成：

```text
stableSubname = Pad.Face5
```

`Pad.Face5` 仍然只是“当前编号路径”，不是 FreeCAD mapped identity。

## 情况二：面只能算 Body-local

例如：

```text
RevolutionBody:Face11
fullSubname = RevolutionBody.Revolution.Face11
```

但 native FreeCAD 检查发现：

```text
Revolution.Shape 只有 Face1..Face4
Revolution.Shape 没有 Face11
```

这时不能说 `Revolution.Face11` 是正式引用。因为 `Revolution` 自己都不认识 `Face11`。

正确理解是：

```text
Face11 是 RevolutionBody 当前结果里的一个 Body-local 面
fullSubname 只是说明显示路径里出现过 Revolution
稳定身份必须属于 RevolutionBody 自己的结果 shape
```

输出应该类似：

```text
subname = Face11
stableSubname = <RevolutionBody.Shape.getElementMappedName("Face11") 的结果>
fullSubname = RevolutionBody.Revolution.Face11
```

如果 `RevolutionBody.Shape.getElementMappedName("Face11")` 拿不到 mapped name，就说明当前没有稳定身份证据，`stableSubname` 应该为空，而不是退回填 `Face11`。

## 哪些写法是错的

错误一：把当前枚举名当稳定身份。

```text
subname = Face11
stableSubname = Face11
```

除非 `NamedShape.elementMap` 或 FreeCAD mapped name 明确证明 `Face11` 是稳定身份，否则这就是把当前编号伪装成稳定语义。

错误二：把 `fullSubname` 裁剪成另一个对象的本地引用。

```text
fullSubname = RevolutionBody.Revolution.Face11
```

不能据此写成：

```text
value = Revolution
SubList = [Face11]
StableSubList = [Face11]
```

只有 `Revolution.Shape.getElement("Face11")` 能成功，并且 `Revolution` 的命名账本也能证明稳定身份时，才能把它当 `Revolution` 的本地引用。

错误三：前端或 adapter 自己拼 stable。

例如看到：

```text
subname = Pad.Face5
```

就自己拼：

```text
stableSubname = Pad.Face5
```

这不对。`stableSubname` 必须来自后端返回的稳定身份，或者 FreeCADCmd / `NamedShape` / `ElementMap` 的明确证据。

## 排查规则

遇到不确定时，不要猜。用 FreeCADCmd 采 native expected：

```bash
cd ~/Chili3DProject/FreeCAD
FREECADCMD=/Users/li/.cargo/bin/FreeCADCmd \
  python3 cad-core/tools/collect_freecad_expected.py \
  path/to/input-or-fixture.json \
  --out cad-core/out/<case>.freecad.json \
  --pretty
```

判断顺序：

1. 先看 `subname` 在当前 `value` 对象里是否能解析。
2. 再看 `fullSubname` 只是显示路径，不能直接当 owner 结论。
3. 最后看 `stableSubname` 是否来自 FreeCAD mapped name / `NamedShape.elementMap`。
4. 如果没有稳定身份证据，就让 `stableSubname` 为空，并把它当本次请求内的临时选择。

一句话总结：

```text
subname 管当前选中
fullSubname 管显示路径
stableSubname 管跨重算找回
```

这三个字段不能混用，尤其不能把 `FaceN` 这种当前编号直接当长期稳定语义。
