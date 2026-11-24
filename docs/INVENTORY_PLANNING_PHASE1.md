# Inventory Planning — Phase 1 MVP (status)

**Дата:** 2026-11-24  
**Спека:** §9 Phase 1

## Сделано

| Пункт | Статус | Где |
|---|---|---|
| DDL `inventory_planning_daily` + `inventory_control_daily` | ✅ на click00 | `docs/sql/inventory_planning_ddl.sql` |
| View с экстраполяцией + low_confidence | ✅ | `v_inventory_planning_daily` |
| Stage table sampled events TTL 14d | ✅ | `docs/sql/inventory_events_stage_ddl.sql` |
| Wins backfill 45d (`RImpression_recent`) | ✅ | 2026-10-10 … 2026-11-23; ~1.74 GiB; ~22.7M keys |
| 5 эталонных SQL | ✅ | `docs/sql/inventory_planning_reference_queries.sql` |
| `inventory_sampling.yaml` + Python helpers | ✅ | `docs/`, `bin/inventory_sampling.py` |
| Async FE InventorySample logger (код) | ✅ код | `LogCommons/InventorySample.*`, `BiddingFrontendLogger`, hook post-parse |

## Осталось до закрытия Phase 1

1. **Сборка + canary** FE logger по `docs/RTB_RELEASE.md` (только `tr*-FCGIRtbServer`, canary `adfe103`).  
2. SyncLogs `InventorySample_*` → adbe00 → ClickHouse uploader → `inventory_events_sample` → batch в куб (`bid_requests_smpl`).  
3. Control L0 100% (FE counters или full_audit L0) + **drift &lt; 3%**.

Пока `bid_requests` в view = 0 (wins-only backfill); imps / avg_win_cpm / bidfloor quantiles уже доступны.

## Проверка wins-куба

```sql
SELECT event_date, sum(imps), count()
FROM default.v_inventory_planning_daily
GROUP BY event_date ORDER BY event_date;
-- yesterday site:
SELECT sum(imps), avg(avg_win_cpm)
FROM default.v_inventory_planning_daily
WHERE event_date = yesterday() AND inventory_type = 'site';
```
