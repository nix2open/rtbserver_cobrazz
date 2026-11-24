# InventorySample — ops (canary)

## Kill-switch / rate (без рестарта кода)

| Действие | Как |
|---|---|
| Выключить сбор | `touch /opt/foros/server/var/run/inventory_sample.disable` на FE |
| Включить снова | `rm -f /opt/foros/server/var/run/inventory_sample.disable` |
| Сменить rate | `echo 0.02 > /opt/foros/server/etc/inventory_sample.rate` (или `0` = off) |

Rate/disable читаются на flush (~period Geo logger).

## Пути

- FE out: `FCGIRtbServer/Out/InventorySample.ram/InventorySample_*.csv`
- SyncLogs → `adbe00:/opt/foros/server/var/log/Predictor/ResearchLogs/InventorySample`
- CH: `default.inventory_events_sample` → rollup `inventory_planning_daily`
