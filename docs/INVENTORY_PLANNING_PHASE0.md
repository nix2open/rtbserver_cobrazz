# Inventory Planning — Phase 0 Discovery

**Дата:** 2026-11-24  
**Спека:** `docs/INVENTORY_PLANNING_SPEC.md` §9 Phase 0  
**Кластер:** colo `programmatic`, CH `click00`, DSP `rtbserver_cobrazz` / `modern`

---

## 1. Политика `bid_request` (зафиксировано)

| Решение | Значение |
|---|---|
| Определение | **post-parse all seen inventory** — каждый impression opportunity после успешного OpenRTB parse в `RequestInfoFiller`, **до** campaign eligibility / bid response |
| Уровень | imp-level (1 row ≈ 1 `imp` opportunity) |
| Не использовать | `ResearchBid` / `RBid` — это только **bid-responded** события (`process_request` после выбора креатива) |
| Не использовать | `RImpression` как source of truth для auctions — только wins/imps path |
| Обоснование | GetIntent «Доступный трафик» / Hybrid Auctions ≈ все увиденные auctions, не только наши bids |

Открытые пункты §14 спеки для v1 принимаем по рекомендациям документа:

| # | Решение v1 |
|---|---|
| Валюта отчёта | RUB (+ native `bidfloor_cur`) |
| Hour cube | **не в v1** |
| Segments | отложить (L3 позже) |
| `default_sample_rate` | **0.02** |
| Wins / imps | **100%** |
| `low_confidence` / drift | sampled_rows &lt; 100; \|drift\| &lt; 3% vs control L0 |
| Ingest | async log/batch → CH; **запрет** семпла на hot bid-path |

---

## 2. Сырой поток OpenRTB dimensions (as-is → to-be)

### As-is

| Поток | Где | Что даёт | Пригоден для planning? |
|---|---|---|---|
| OpenRTB parse | `Frontends/Modules/BiddingFrontend/RequestInfoFiller.cpp` | полный site/app/device/user/imp | да, как источник полей |
| FE metrics | `http://127.0.0.1:10277/metrics` (`rtb_request_total`, `rtb_requests_total{path="/openrtb"}`) | QPS / funnel | control totals L0 (счётчики), без dims |
| `ResearchBid` → `RBid_*` | RIM Out → SyncLogs `/ResearchBid` | bid + floor + tag/colo/size | **нет** (только отвеченные bids) |
| `ResearchImpression` → `RImpression` | → CH `default.RImpression` / `_recent` | wins + `bid_floor` + `win_price` | **imps/wins only** |
| `PublisherInventory` | CampaignManager billing stat | pub imps/requests/revenue | другой продукт, не OpenRTB cube |
| Kafka raw bid | нет готового топика под inventory dims | — | не найден |

### To-be (обязательно для Phase 1+)

Новый **async** sampled inventory log с FE (после parse, off hot path):

1. Deterministic sample: `cityHash64(request_id) % 1_000_000 < rate * 1_000_000`  
2. Flush CSV/TSV → SyncLogs → batch upload → CH stage → MV/batch в `inventory_planning_daily`  
3. Параллельно L0 control counters (100%) — либо из FE sketches, либо из того же лога с `sample_tier=full_audit` только L0 dims на короткий TTL / отдельный counter path  

Wins path: существующий `RImpression` → adapter → куб с `sample_rate=1.0` для `imps` / `win_*` / `spend`.

---

## 3. Mapping OpenRTB / RequestInfoFiller → колонки куба

| Куб (P0/P1) | OpenRTB / filler | Примечание |
|---|---|---|
| `ssp_id` / `colo_id` | source / `colo_id` | в CH сейчас `colo_id`; UI-имя `ssp_id` |
| `inventory_type` | наличие `site` vs `app` | `site`\|`app`\|`unknown` |
| `publisher_id` | `site.publisher.id` / `app.publisher.id` / account | |
| `domain` | `site.domain` / referer host | normalized host; **не** page_url |
| `app_bundle` / `app_id` / `app_name` | `app.bundle` / `app.id` / `app.name` | |
| `tag_id` / `ssp_tag_id` | internal tag / `imp.tagid` | |
| `country` / `region` / `city` | `device.geo.*` (+ geo channels) | country FixedString(2) |
| `device_type` | `device.devicetype` → string map | `openrtb_devicetype_mapping_` |
| `os` | `device.os` / `device.osv` | osv → P1 |
| `ad_format` | banner / video / native presence | |
| `size` | `banner.w×h` / `video.w×h` / `size_id` | |
| `deal_id` | `imp.pmp.deals.id` | P1 |
| `user_known` | buyeruid / uid / ifa present | |
| `bidfloor` / `bidfloor_cur` | `imp.bidfloor` / `imp.bidfloorcur` | |
| `uid_hash` | buyeruid \| user.id \| ifa | только hash в long-term |
| `carrier` / `device_lang` | `device.carrier` / `device.language` | P1 |
| `secure` / `instl` / `skippable` / `rewarded` | `imp.secure` / `imp.instl` / `video.skip` / `video.reward` | P1 |
| `win_price` | win notice → ResearchImpression | wins path only |

Анти-паттерн подтверждён: `page_keywords` / raw URL path / segments — **не** в L1 ключ.

---

## 4. `win_price` / `bid_floor` на пути

Проверено на `click00` / `default.RImpression_recent` за `yesterday()` (2026-11-23):

| Метрика | Значение |
|---|---:|
| imps | 8 350 790 |
| `bid_floor > 0` | 7 714 491 (**92.4%**) |
| `win_price > 0` | 8 350 790 (**100%**) |
| p05 floor (nonzero) | ≈ 0.018 |
| avg win_price | ≈ 0.358 |
| domains | ≈ 19 252 |
| distinct L1-like keys | ≈ **581 369** |

Схема `RImpression`: колонки `bid_floor`, `win_price`, `bid_price`, `colo_id`, `publisher_id`, `tag_id`, `ssp_tag_id`, `size_id`, `device`, `uid`, `url`, `geo_ch` — присутствуют.  
Adapter: `bin/RImpressionClickhouseAdapter.py`.

---

## 5. QPS bid ingress и объём при rate=0.02

Замер FE metrics (`/metrics` port `10277`), окно ~5–20s, 2026-11-24:

| Host | ≈ openrtb RPS (`rtb_request_total` Δ) |
|---|---:|
| adfe100 | 5 575 |
| adfe103 | 5 888 |
| adfe200 | 5 018 |
| adfe205 | 6 134 |
| adfe101/104/105/201–203 | counters live (тот же порядок) |

В SyncLogs перечислены **21 FE** (`adfe100–110` без 102 + `adfe200–211` без 204).

**Оценка фермы:** ~5.5k × 21 ≈ **~115k RPS** ≈ **~10B requests/day**.

| Сценарий | Events/day | Следствие |
|---|---:|---|
| 100% raw 45d | ≫ TiB | запрещено (§5) |
| **2% sample** | ~200M events/day ingress to agg | CPU/сеть заметны → только async batch |
| после daily cube | keys ≈ union(wins≈0.58M, sampled bids) | ориентир **верхняя** зона §5.4 (десятки–~100+ GiB) — нужен мониторинг бюджета §5.6 |
| wins full | ~8.3–8.7M/day | ок |
| control L0 100% | counters / coarse key | ≪ 1 GiB |

Canary funnel (adfe103, 20s snap): finished ≈ 8.7% запросов, bid response ≈ 0.055%, timeout ≈ 93%.  
→ `ResearchBid` покрывает **≪ 1%** ingress — непригоден как available inventory.

Отношение bid_requests/imps на проде (п.9 §14): **порядка 10²–10³** (зависит от того, считаем ли timeout’ы «seen»). Для planning v1 считаем **все post-parse** HTTP OpenRTB requests с ≥1 imp opportunity; уточнение imp-vs-request multiplicity — в ingest (1 sample row per imp).

---

## 6. Выводы для Phase 1

1. DDL `inventory_planning_daily` + `inventory_control_daily` + view — по §6.  
2. Wins backfill 45d из `RImpression_recent` (есть с 2026-08-23).  
3. Bid-request sample path — **новый** async logger + uploader (не RBid); до выката логгера control L0 можно питать из FE counter snapshots, L1 bids — накопление с даты включения.  
4. Конфиг: `docs/inventory_sampling.yaml`.  
5. Деплой изменений FE — только через `docs/RTB_RELEASE.md` (canary `adfe103` → cmgr).

---

## 7. Риски, подтверждённые discovery

| Риск | Статус |
|---|---|
| RImpression не planning SoT | подтверждён (ORDER BY request_id, PARTITION hash) |
| Кардинальность domain×tag | ~0.58M keys/day только wins |
| Ingress ≫ wins | подтверждён метриками FE |
| Семпл на hot path | запрещён; logger async only |
| Drift vs control | обязателен с первого дня семпла |
