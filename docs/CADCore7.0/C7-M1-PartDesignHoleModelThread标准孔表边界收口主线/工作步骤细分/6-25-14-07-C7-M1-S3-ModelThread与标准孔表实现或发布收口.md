# C7-M1 S3 ModelThread 与标准孔表实现或发布收口

## 目标

执行 S2 裁决。若 S2 标出 active backend gap，则按 FreeCAD 源码链补实现；若没有 active backend gap，则只做 publication closure，不改几何实现。

## 必读

- S2 已实现步骤文件
- `src/Mod/PartDesign/App/FeatureHole.cpp`
- `cad-core/src/part_design/feature_hole.cpp`
- 需要时读取 `cad-core/src/part/topo_shape*`、`cad-core/src/topo/*`
- `cad-core/tests/test_p7_features.py`
- `cad-core/tests/test_adapters.py`

## 动作

1. 如果 S2 没有 `backend_gap_requires_implementation`，只同步 capability/test/docs publication assertion 和矩阵状态，不改 C++ 主路径。
2. 如果 S2 接受 backend gap，先在步骤中写 FreeCAD 调用链和 cad-core 分层映射，再改代码。
3. 实现必须落在 `feature_hole.cpp` 或必要的 topo/history API；不得通过 fixture 名称、几何排序猜测或输出端修剪补业务逻辑。
4. 新增或修改公共语义类型/字段时，在相邻 C++ 注释写明 FreeCAD 源文件、函数和关键短句/字段名。
5. 更新 focused tests；如果新增 expected，必须说明 FreeCAD oracle 采集环境。

## 验收

```bash
cd /Users/li/Chili3DProject/FreeCAD/cad-core
cmake --build build
python3 -m unittest \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_supported_threaded_heads_match_native_oracle \
  tests.test_p7_features.CadCoreP7FeatureTest.test_p7_hole_model_thread_builds_freecad_pipe_shell_tool \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_thread_table_model_thread_contract_uses_native_oracles \
  tests.test_p7_features.CadCoreP7FeatureTest.test_c3m5_hole_threaded_model_thread_head_cut_oracle_matrix_matches_native
```

文档和矩阵仍需执行：

```bash
cd /Users/li/Chili3DProject/FreeCAD
awk -F '\t' 'FNR==1{n=NF; next} NF!=n{print FILENAME ":" FNR ": expected " n " fields, got " NF; bad=1} END{exit bad}' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线/矩阵/*.tsv
rg -n '[ \t]$' docs/CADCore7.0/C7-M1-PartDesignHoleModelThread标准孔表边界收口主线
```

## 通过条件

- S2 接受的 rows 已实现或明确 no-code closure。
- Focused tests 与 capability assertion 同步。
- S3 文件名和标题标记为 `【已实现】` 后，队列推进到 S4。
