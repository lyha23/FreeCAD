# topoNamingState 客户端携带状态接口方案

## 结论

CAD Core 采用“服务端无会话、协议有状态”的拓扑命名方案。服务端不保存 session，也不把旧 shape 缓存在后端；调用方每次把上一次响应中的完整 `topoNamingState` 原样带回。CAD Core 用当前 `DocumentObject graph` 重新计算几何结果，并用旧 `topoNamingState` 辅助旧引用恢复、split / deleted / ambiguous 判断和引用更新建议。

## 边界

- `DocumentObject graph` 仍是唯一建模事实。
- `topoNamingState` 不是建模几何输入，不能用于构造 shape、bbox、volume、mesh 或完整对象 BREP。
- `topoNamingState` 可以携带旧 `NamedShape`、`ElementMap`、child element map、mapper history、raw mapped name、canonical mapped name、object hash 和版本信息。
- 除 `ReferenceShadow.brep` 这个单个被引用 subshape 的旧快照外，不在 state 中保存完整对象 BREP。
- 服务端每次返回新的完整快照；调用方以下一次响应替换旧快照，不做客户端合并。

## 请求

```json
{
  "Objects": [],
  "recompute": {
    "objs": ["BooleanFragments"]
  },
  "topoNamingState": {
    "schemaVersion": "cad-core.topo-state.v1",
    "producer": {
      "cadCoreVersion": "",
      "freecadVersion": "",
      "occtVersion": ""
    },
    "documentHash": "",
    "objects": {}
  }
}
```

`topoNamingState` 可为空或缺省。首次 recompute 没有旧 state 时，CAD Core 只根据 graph 计算，并在响应中返回新 state。只要请求携带了 `topoNamingState`，CAD Core 必须先校验 `schemaVersion` 和 `producer` 兼容性；版本不兼容时直接返回请求级错误，不继续 recompute 当前 graph，也不把旧 state 当作可降级证据消费。

## 响应

```json
{
  "results": [],
  "topoNamingState": {
    "schemaVersion": "cad-core.topo-state.v1",
    "producer": {
      "cadCoreVersion": "",
      "freecadVersion": "",
      "occtVersion": ""
    },
    "documentHash": "",
    "objects": {
      "BooleanFragments": {
        "objectHash": "",
        "elementMapVersion": "",
        "subshapes": {
          "Face1": {
            "subname": "Face1",
            "rawFreecadMappedName": "Face1;:H551,F;CSD;:H551:4,F;:H,F;:H:5,F",
            "canonicalFreecadMappedName": "Face1;:H*,F;CSD;:H*:*,F;:H,F;:H:5,F",
            "resolvedIndexed": "Face1",
            "identityStatus": "stable"
          }
        },
        "elementMap": {
          "encoding": "cad-core.element-map.v1",
          "status": "history_partial",
          "entries": {}
        },
        "childElementMaps": [],
        "mapperHistory": []
      }
    }
  },
  "elementReferenceUpdates": [],
  "diagnostics": []
}
```

## 字段语义

| 字段 | 语义 |
| --- | --- |
| `schemaVersion` | state schema 版本。当前接口不做向后兼容；不等于 CAD Core 支持版本时直接报错。 |
| `producer` | 生成 state 的 CAD Core / FreeCAD / OCCT 版本。用于兼容性判断；不在当前 CAD Core 接受范围内时直接报错。 |
| `documentHash` | 当前 graph 的规范化 hash。用于判断旧 state 是否属于同一文档族。 |
| `objectHash` | 单对象输入语义 hash。对象属性、链接或依赖变化时会变化。 |
| `elementMapVersion` | 对齐 FreeCAD ElementMap version / CAD Core topo ledger version。 |
| `subshapes` | 当前响应发布的 indexed subshape 到稳定证据的映射。 |
| `rawFreecadMappedName` | FreeCAD / CAD Core 原始 mapped-name token。只有配套 state 存在并校验通过时才可作为恢复证据。 |
| `canonicalFreecadMappedName` | 抹平 hash / delete 片段漂移的比较 key，只用于 expected / diff 稳定性，不是原生输入。 |
| `resolvedIndexed` | 生成本 state 时 raw token round-trip 到的当前 indexed subname。 |
| `elementMap` | 可序列化的 ElementMap 主账本；只表达 stable token 到当前唯一 indexed subname 的映射。 |
| `childElementMaps` | Link、Compound、Compsolid、Body/Tip、ShowElement 等子账本。 |
| `mapperHistory` | split、deleted、merge、generated、modified 等非唯一或历史事件。 |

## elementMap 协议形态

`elementMap` 只承载已经唯一恢复的映射：`entries[stableToken].target.subname` 必须是当前 recompute 结果中存在的 indexed subname。split、deleted、ambiguous 或 needs-reselect 这类非唯一关系不得塞回 `elementMap`，必须通过 `mapperHistory` 和 diagnostics 表达。

```json
"elementMap": {
  "encoding": "cad-core.element-map.v1",
  "status": "history_partial",
  "entries": {
    "Sketch.Edge1;:H551,E;CSD;:H551:2,E": {
      "target": {
        "object": "BooleanFragments",
        "subname": "Edge7"
      },
      "shapeKind": "edge",
      "source": {
        "object": "Sketch",
        "subname": "Edge1"
      },
      "mappedName": {
        "raw": "Sketch.Edge1;:H551,E;CSD;:H551:2,E",
        "canonical": "Sketch.Edge1;:H*,E;CSD;:H*:*,E"
      },
      "recoverability": "resolved",
      "evidence": {
        "source": "element_map",
        "mapperHistoryIds": ["mh-001"],
        "childElementMapKey": null
      }
    }
  }
}
```

字段规则：

- `encoding` 是 `elementMap` 内部编码版本。当前接口不做向后兼容，编码版本不匹配时按 state 版本不兼容处理。
- `status` 表示账本覆盖状态，可取 `indexed_only`、`history_partial`。它只用于诊断和验收，不允许客户端据此补猜映射。
- `entries` 的 key 是可写入 `StableSubList` 的 stable token。客户端只保存和回传，不解析、不合并、不重写。
- `target.object` 和 `target.subname` 指向当前响应中唯一可恢复的对象和 indexed subname。
- `shapeKind` 取 `face`、`edge`、`vertex`、`wire`、`shell`、`solid`、`compound` 等拓扑类型，用于校验引用类型是否匹配。
- `source` 是来源证据，不是几何输入；对象改名、Link retag 或 child map 展开后可以和 `target.object` 不同。
- `mappedName.raw` 是原始 mapped-name token，只有配套 state 校验通过时才可作为恢复证据。
- `mappedName.canonical` 只用于 expected / diff 稳定性，不是可解析输入。
- `recoverability` 在 `elementMap` 中只能是 `resolved` 或 `recoverable` 这类仍可唯一落到 `target` 的状态；`deleted`、`ambiguous`、`needs_reselect` 不得进入 `entries`。
- `evidence.mapperHistoryIds` 只把唯一映射和 `mapperHistory` 事件串起来，方便诊断和验收；不得让客户端依赖该字段自行重放历史。
- 不允许用 `target: null`、`target: []`、多 target 数组或客户端排序规则表达 split / deleted / ambiguous。

## StableSubList 规则

`StableSubList` 可以保存 state-backed token：

```json
{
  "PropertyType": "App::PropertyLinkSub",
  "value": "BooleanFragments",
  "SubList": ["Face1"],
  "StableSubList": ["Face1;:H551,F;CSD;:H551:4,F;:H,F;:H:5,F"],
  "StableSubListSource": "topoNamingState"
}
```

解析顺序：

1. 用当前 graph recompute 目标 shape。
2. 校验传入 `topoNamingState` 的 schema、producer、document / object hash；schema / producer 版本不兼容时直接返回请求级错误，后续步骤不执行。
3. 用 `StableSubList` 在旧 state 中定位旧 mapped-name / history 证据。
4. 用当前 shape 的新 ElementMap / mapper history 尝试恢复到 current indexed subname。
5. 成功时返回新的 `SubList`、`StableSubList`、`ShadowSub`、`ReferenceShadow` 和 `topoNamingState`。
6. 失败时返回 `deleted_stable_subname`、`split_stable_subname`、`ambiguous_stable_subname` 或 `unsupported_stable_subname`，不得靠 bbox、顺序或 fixture 名称猜。

## raw / canonical / stable 的区别

- `rawFreecadMappedName`：账本原始 token，可能包含 `:H551`、`D233a`、`CSD` 等片段。
- `canonicalFreecadMappedName`：把会漂移的片段规范化后的比较 key，例如 `D233a -> D*`。它不是 FreeCAD 原生输入。
- `stableSubname`：对外可作为引用输入的 token。只有在配套 `topoNamingState` 证据存在、版本匹配、且恢复路径可验证时才发布为长期 stable。

## 安全与版本策略

- 前端传回的 state 不可信。CAD Core 必须验证 schema、producer、document hash、object hash 和 property ownership。
- state 只能影响引用恢复和 diagnostics，不能影响几何构造结果。
- 当前接口不承诺向后兼容旧版 `topoNamingState`。`schemaVersion`、CAD Core 版本、FreeCAD 版本或 OCCT 版本不在当前接受范围内时，CAD Core 直接返回请求级错误。
- 版本不兼容错误是硬失败：不继续 recompute 当前 graph，不返回新的 `topoNamingState`，不把旧引用恢复降级为 diagnostic，也不按当前版本尝试解析旧 state。
- state payload 可能很大，后续可压缩传输，但压缩格式不得改变语义 schema。

## 非目标

- 不引入服务端 session / 数据库 / 用户缓存。
- 不保存完整对象 BREP 作为长期状态。
- 不把 `canonicalFreecadMappedName` 当作可解析 subname。
- 不允许 executor、adapter 或输出层按几何形态猜旧引用归属。

## 验收命令

本轮短跑：

```bash
git diff --check -- docs/CADCore方案/00-CAD-Core抽取方案.md \
  docs/CADCore方案/细化方案/03-接口与验收样例.md \
  docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md
```

阶段回归在实现 DTO / runtime state 后再补：

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
python3 -m unittest tests/test_expected_fixtures.py
```
