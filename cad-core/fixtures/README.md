# Fixture expected 文件约定

`fixtures/<phase>/*.json` 是 CAD Core 输入 fixture。对应的期望文件放在
`fixtures/<phase>/expected/`，但期望文件按来源分成两类。

## `.freecad.json`

`.freecad.json` 只用于 FreeCADCmd 采集、FreeCADCmd 等价 oracle，或已经明确标注
FreeCAD native parity 的几何期望。

这类文件可以被 `cad-core/tools/collect_freecad_expected.py --check` 和
`cad-core/tests/fixture_expected.py` 的默认发现逻辑当作 FreeCAD oracle 使用。

如果文件里包含 `rawFreecadMappedName`，它表示 FreeCADCmd 的原始 mapped name，
只用于 oracle/debug 对照；`stableSubname` 才是去掉进程内易变 hash token 后的
稳定身份契约。

## `.expeted.json`

`.expeted.json` 用于人工维护的 CAD Core 产品契约或诊断契约，不表示
FreeCADCmd 原生采集输出。当前该后缀按现有约定拼写为 `expeted`。

这类文件常见于缺少可运行 native oracle、FreeCAD helper 暂不可采集、或 fixture
本身用于约束 CAD Core 诊断行为的场景。它可以记录 `diagnostic_codes`、
`oracle_evidence.freecad_native_parity=false` 和原因，但不能当成 FreeCAD native
几何标准答案。

如果后续某个 `.expeted.json` case 能被 FreeCADCmd 稳定采集，应重新运行
collector 生成 `.freecad.json`，再删除或替换原来的人工契约文件。

不要新增 `.cad-core.json` 这类第三种 expected 后缀。已有 CAD Core 人工契约应统一改为
`.expeted.json`，避免测试发现和 oracle 语义继续分叉。
