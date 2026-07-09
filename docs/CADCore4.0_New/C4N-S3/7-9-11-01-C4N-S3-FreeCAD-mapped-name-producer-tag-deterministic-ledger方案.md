# C4N-S3 FreeCAD mapped-name producer tag deterministic ledger 方案

## 目标

在 C4N-S1/S2 已关闭 c4m6、p2、p6 focused topoNamingState parity 后，本批只处理下一层问题：FreeCAD mapped-name raw token 里的 producer tag 必须在 CLI / C API / worker / wasm 等不同 cad-core 入口之间保持确定性。

当前不能直接把 `tests.test_expected_fixtures tests.test_adapters` 当作全量通过门槛。该 broad suite 还混有旧 schema、缺失 expected、非 topoNamingState adapter drift 和 product fixture 差异。本批先建立准入矩阵，把真正属于 topoNamingState producer tag 的问题收敛出来。

## 当前基线

- 协议权威仍是 `docs/接口规定/7-8-11-08-topoNamingState客户端携带状态接口方案.md`。
- C4N-S1 已关闭 c4m6 focused state schema、child map、mapperHistory、hard-fail 和 ReferenceShadow 边界。
- C4N-S2 已关闭 p2 / p6 producer mapped-name expectedFailure。
- 当前 `cad-core/fixtures/*/expected/*.freecad.json` 约 503 个 native expected 文件；本批不手改这些文件。
- 上一轮阶段收口中暴露的关键新问题是同一 topoNamingState response 在 CLI / C API / worker / wasm 之间 raw `:H...` tag 不一致，说明 producer tag 仍依赖入口或进程内 shape hash。

## FreeCAD 依据

本批实现必须继续以 FreeCAD 的 ElementMap / StringHasher 路径为依据：

- `src/App/ElementMap.cpp::ElementMap::encodeElementName()`：组合 source token、postfix、master tag、source tag 和 element type。
- `src/App/ElementMap.cpp::ElementMap::hashElementName()`：只有包含 element map prefix 的名称进入 StringHasher。
- `src/App/StringHasher.cpp::StringHasher::getID()`：建立 request-local string id。
- `src/App/StringHasher.cpp::StringID::toString()`：输出 `#<hex>` 或 `#<hex>:<index>`。
- `src/Mod/Part/App/TopoShapeExpansion.cpp::TopoShape::makeShapeWithElementMap()`：producer ledger 在 maker 过程中写入 ElementMap，后续 refine / preserved pass 消费已有 ledger。

## 代码落点

| 层 | 落点 | 本批职责 |
| --- | --- | --- |
| topo codec | `cad-core/src/topo/freecad_mapped_name_codec.cpp` | 明确 raw / canonical token 编码规则，避免入口差异影响 raw。 |
| part/topo ledger | `cad-core/src/part/topo_shape.cpp` | 用 request-local deterministic ledger 替换依赖 OCCT `TopoDS_Shape` hash 的 producer tag 来源。 |
| runtime publication | `cad-core/src/runtime/topo_naming_state.cpp` | 继续只发布 source-backed mapped-name entry，不从 display/stable token 伪造 raw。 |
| adapters | `cad-core/tests/test_adapters.py` | 固化 CLI / C API / worker / wasm 的 topoNamingState channel 一致性。 |
| focused tests | `cad-core/tests/test_topo_naming_state_response.py` | 保持 C4N-S1/S2 focused parity 不回退。 |

## 实现原则

1. 先做语料准入，再落代码。不要把旧 schema、missing expected、geometry bbox drift 和 adapter result contract drift 混进 producer tag 问题。
2. producer tag 必须是本次 recompute 内的确定性 ledger 结果，不能依赖进程地址、OCCT hash seed、adapter 执行顺序或序列化偶然性。
3. raw mapped name 保持 FreeCAD 风格；canonical 只用于 expected / diff 稳定性。
4. 不通过测试层 canonical 化来掩盖 raw tag 漂移。跨入口 raw 不一致时，先修 producer ledger。
5. 不手改 native expected。若发现 collector 或 expected schema 本身错误，先修 collector，再用 `FreeCADCmd` 重新采集。

## 非目标

- 不在本批追全量 `tests.test_expected_fixtures tests.test_adapters` 绿灯。
- 不继续扩大 p2/p6 静态 seed。
- 不把 `topoNamingState` 变成服务端 session 或几何输入。
- 不用 bbox、fixture 名称、输出顺序或 adapter 后处理猜 raw mapped name。

## 验收

### 本轮短跑

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest tests.test_topo_naming_state_response
python3 -m unittest \
  tests.test_adapters.CadCoreAdapterTest.test_c13m1_cli_c_api_worker_wasm_share_topo_naming_state_channel \
  tests.test_adapters.CadCoreAdapterTest.test_c4s11_cli_c_api_worker_wasm_share_core_result_contract
```

### 文档检查

```bash
cd /Users/li/Chili3DProject/FreeCAD
git diff --check -- docs/CADCore4.0_New cad-core
```

### 阶段收口

只有 producer tag 跨入口一致后，才扩大到全 corpus 准入清单中标记为 topoNamingState-relevant 的 fixture。

## 完成判定

- CLI / C API / worker / wasm 对同一请求返回的 `topoNamingState` raw mapped-name 完全一致。
- C4N-S1/S2 focused tests 继续通过。
- C4N-S3 矩阵明确区分 producer tag blocker、旧 schema/missing expected blocker、非 topoNamingState product drift。
- README 指向 C4N-S3，并且 C4N-S1 旧 open/pending 状态不再误导后续 worker。
