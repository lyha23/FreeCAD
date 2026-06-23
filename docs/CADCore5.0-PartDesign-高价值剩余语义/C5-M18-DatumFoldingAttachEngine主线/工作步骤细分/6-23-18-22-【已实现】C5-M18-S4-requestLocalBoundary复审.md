# 【已实现】C5-M18-S4 request-local boundary 复审

状态：`done_s4_closed`

## 边界

- 后端只消费本次 request graph 中的 support object / subname / shape snapshot。
- `AttachmentSupport` writeback 仍是 response suggestion，不是 backend session。
- `MapMode`、`AttachmentOffset`、`MapReversed` / `Reverse` 只影响本次 recompute 输出。
- 不保存完整 shape 或 BREP；`ReferenceShadow.brep` 仍只允许保存单 referenced subshape snapshot。

## C5-M18 写回

`Folding` 使用四个 supports，仍走已有 request-local support parsing / writeback suggestion 机制；它没有新增跨请求状态或持久几何缓存。

## 验收

```bash
cd /home/user/Chili3DProject/FreeCAD/cad-core
./cad-core recompute fixtures/c51m5/partdesign-datum-folding-modes.json --output /tmp/c5m18-folding.result.json
python3 -m json.tool /tmp/c5m18-folding.result.json >/dev/null
```
