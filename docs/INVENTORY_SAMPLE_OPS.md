# InventorySample — ops (без пересборки)

Управление сбором bid-сэмпла для inventory planning. Работает через файлы, которые
`BiddingFrontendLogger` читает на каждом flush (~period Geo, обычно ~10s).
**Рестарт FCGIRtbServer / пересборка RPM не нужны.**

Требуется бинарь **≥ ssv684** (kill-switch + отдельный `InventorySample.ram`).

## Команда

Скрипт: `/opt/foros/server/bin/inventory_sample_ctl.sh`  
(в репо: `bin/inventory_sample_ctl.sh`)

```bash
# на FE (root)
inventory_sample_ctl.sh status          # disable/rate/хвост файлов
inventory_sample_ctl.sh off             # остановить сбор
inventory_sample_ctl.sh on              # снова включить
inventory_sample_ctl.sh rate 0.02       # целевой sample rate
inventory_sample_ctl.sh rate 0          # = не сэмплировать
inventory_sample_ctl.sh purge           # удалить хвост CSV (RAM + legacy Geo)
inventory_sample_ctl.sh space           # df / du по RAM FS и out-dir
```

С ноутбука на canary:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@adfe103 \
  /opt/foros/server/bin/inventory_sample_ctl.sh off
```

## Что меняется под капотом

| Действие | Файл | Эффект |
|---|---|---|
| `off` | `touch /opt/foros/server/var/run/inventory_sample.disable` | сбор стоп |
| `on` | `rm` этого файла | сбор снова |
| `rate X` | `/opt/foros/server/etc/inventory_sample.rate` (одно float 0..1) | override rate; `0` = off |
| нет rate-файла | — | default из бинаря (сейчас `0.02`) |

## Если забивается место на FE (RAM FS)

Out идёт в **отдельный** каталог, не в `Geo.ram`:

`/opt/foros/server/var/log/FCGIRtbServer/Out/InventorySample.ram`  
→ `/dev/rtbserver_ram_fs/log/FCGIRtbServer/Out/InventorySample`

Порядок при аварии (место кончается / SyncLogs отстаёт):

1. **Сразу выключить сбор** — `inventory_sample_ctl.sh off`  
   (до ssv684: `touch /tmp/inventory_sample_cleanup_stop` не останавливает запись;
   только cleaner или рестарт со старым бинарём не поможет — нужен disable из ssv684
   либо `purge` в цикле).
2. **Сбросить хвост** — `inventory_sample_ctl.sh purge`
3. Проверить `space` / `df -h /dev/rtbserver_ram_fs`
4. Если SyncLogs/uploader на `adbe00` не забирают файлы — чинить доставку, не поднимать rate
5. После разгрузки — `on` и при необходимости `rate 0.01` / `0.02`

Пока canary на **ssv683** (баг ~100% + писало в Geo): cleaner на adfe103 удаляет
`InventorySample_*.csv` из Geo каждые 15s. После деплоя ssv684 —  
`touch /tmp/inventory_sample_cleanup_stop` и дальше только `inventory_sample_ctl.sh`.

## Пути доставки

| Этап | Путь |
|---|---|
| FE out | `FCGIRtbServer/Out/InventorySample.ram/InventorySample_*.csv` |
| SyncLogs | `adbe00:/opt/foros/server/var/log/Predictor/ResearchLogs/InventorySample` |
| CH stage | `default.inventory_events_sample` |
| Rollup | `docs/sql/inventory_events_to_planning.sql` → `inventory_planning_daily` |

Canary SyncLogs route сейчас ограничен **adfe103** (hot-patch на adbe00).
В CMS (`SyncLogs.xsl`) маршрут на все FE — попадёт в config RPM при следующем cfgen.

## Быстрый чеклист «хвост не копится»

```bash
# FE
inventory_sample_ctl.sh status
inventory_sample_ctl.sh space

# BE
ls /opt/foros/server/var/log/Predictor/ResearchLogs/InventorySample | tail
clickhouse-client -h click00 -q \
  "SELECT count(), max(event_time) FROM default.inventory_events_sample"
```
