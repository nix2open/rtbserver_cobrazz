# ТЗ: система анализа доступного инвентаря (DSP Inventory Planning)

**Статус:** аналитика + ТЗ для реализации ИИ-агентом  
**Дата:** 2026-11-24  
**Контекст:** живая DSP Genius Desk (`rtbserver_cobrazz` / ветка `modern`), ClickHouse `click00`  
**Горизонт данных:** скользящие **45 дней**, агрегация **по дням** (опционально hour для детализации)  
**Объём:** без 100% raw на 45d — семпл bid_requests **2%** + full wins + L0 control (§5); ориентир куба **~30–100 GiB / 45d**

---

## 1. Цель

Построить в ClickHouse витрину планирования, которая по любому комбинаторному срезу (как в Hybrid Insights / GetIntent «Доступный трафик») отвечает на вопросы:

| Вопрос планирования | Метрика |
|---|---|
| Сколько аукционов / bid request доступно? | `bid_requests` |
| Какой примерный reach? | `uniq_users` (HLL / approx) |
| Сколько показов реально выкупили? | `imps` |
| С какого пола можно входить? | `min_bidfloor_cpm` (из запроса) |
| Сколько в среднем платим при выкупе? | `avg_win_cpm` (по win/clearing price) |

Система должна учитывать, что у нас **своя живая DSP**: источник истины — наш bid-path (OpenRTB ingest → decision → win/impression), а не внешний SaaS. Внешние продукты (Hybrid Insights, GetIntent LiveReports / u-Slicer) используются как **референс UX и каталога срезов**.

---

## 2. Источники анализа (проведено)

### 2.1 Hybrid Insights — https://insights.hybrid.ai/

UI: Imply Pivot `Dsp Traf` (iframe `pivot.hybrid.ai`).  
Метрика по умолчанию: **Auctions** (~1.81b за latest hour в demo).

**Срезы (DIMENSIONS) из правого/левого меню:**

#### Time
- Time (granularity: hour / day / …)

#### Placement
| UI | OpenRTB / смысл |
|---|---|
| SSP | exchange / source / seat (наш `colo` / SSP id) |
| Type (browser / app / …) | наличие `site` vs `app` |
| Publisher ID | `site.publisher.id` / `app.publisher.id` |
| Domain | `site.domain` / `app.domain` |
| IAB category | `site.cat` / `app.cat` / content.cat |
| Google category | SSP ext (если есть) |
| Device language | `device.language` |
| Page language | page / content language (если есть) |
| PlacementId | `imp.tagid` |

#### Geo
| UI | OpenRTB |
|---|---|
| Country / State / City | `device.geo.country/region/city` |
| Postal code | `device.geo.zip` |
| Has location | lat/lon present |

#### Mobile carrier
- Mobile carrier (IAB) ← `device.carrier`
- Mobile carrier (Google) ← SSP mapping

#### Mobile application
- App name ← `app.name`
- App ID ← `app.id` / `app.bundle`
- App category ← `app.cat`

#### Data (аудитории)
- Data providers, Aidata, Soloway (Adriver), 1DMC CleverData, OnAudience, Digiseg
- Context channels, Remarketing, Places, Hybrid segments
- Age, Gender ← `user.yob` / `user.gender` / segments

#### Banner / creative opportunity
| UI | OpenRTB |
|---|---|
| Format | banner / video / native / audio |
| Placement size | `banner.w×h` / `video.w×h` / format[] |
| Placement position | `banner.pos` / `video.pos` |
| Skippable | `video.skip` |
| Interstitial | `imp.instl` |
| Rewarded | `video.reward` / ext |
| MRAID | `banner.api` / `video.api` |
| Native / Native video | `imp.native` |
| Vertical video | aspect / placement |
| Video linearity | `video.linearity` |
| Video type / start delay | `video.*` / `startdelay` |
| Viewability | `imp.metric` (viewability) |
| Historical CTR | metric / derived |

#### Device
| UI | OpenRTB |
|---|---|
| Device | `device.devicetype` |
| Has Device ID | ifa / did* / dpid* present |
| Device maker / model | `device.make` / `device.model` |
| Connection | `device.connectiontype` |
| OS / OS version | `device.os` / `device.osv` |
| Browser / Browser version | UA parse / `device.ua` |

**Вывод для нас:** Hybrid — эталон **широкого каталога срезов** для планирования «до ставки». Метрика Auctions ≈ bid requests.

### 2.2 GetIntent LiveReports — dataset `available_inventory`

URL-паттерн:  
`https://ui.getintent.com/livereports?dataset_name=available_inventory&keys=day&period=yesterday&filter=eq:is_app:false`

Dataset UI: **«Доступный трафик»**.  
TZ по умолчанию: `Europe/Moscow (UTC+03:00)`.

#### Группировки (keys), доступные в UI

| UI (RU) | Рекомендуемый field name |
|---|---|
| Дата / Час / Месяц | `day`, `hour`, `month` |
| Операционная система | `os` |
| SSP | `ssp` |
| Заблокированные рекламодатели | `blocked_adv` |
| Город / Район / Регион / Страна / ZIP | `city`, `district`, `region`, `country`, `zip` |
| Blocked attributes | `blocked_attr` |
| Приложение (ID) | `app_id` |
| Домен / Домен верхнего уровня | `domain`, `tld` |
| В моб. приложении | `is_app` |
| Tag (ID) | `tag_id` |
| Браузер | `browser` |
| Бид на сделку (ID) / Available deal (ID) | `deal_bid_id`, `available_deal_id` |
| Видео / Rewarded Video / Пропускаемое | `is_video`, `is_rewarded`, `is_skippable` |
| Адрес страницы | `page_url` |
| Домен скрыт SSP | `domain_masked` |
| Нативные креативы | `is_native` |
| Пользователь известен | `user_known` |
| Расположение на экране | `ad_position` |
| Тип устройства | `device_type` |
| IBV blocked | `ibv_blocked` |
| Размер плеера / Размер креатива | `player_size`, `creative_size` |
| Publisher (ID) / Publisher's website (ID) | `publisher_id`, `publisher_site_id` |
| Координаты доступны | `has_geo` |
| Сегменты | `segments` |
| Оператор | `carrier` |
| Категории контента | `content_cat` |
| Keywords | `keywords` |
| Video Ad Placement / Формат видеотрафика | `video_placement`, `video_inventory_format` |

#### Метрики (data fields) dataset

| UI | Смысл для планирования |
|---|---|
| **Доступные показы** | объём доступного inventory (≈ auctions / opportunities) |
| Показы | выкупленные imps |
| Уникальные показы | approx unique |
| Клики / CTR / CPC | post-win quality |
| **CPM** | средний CPM выкупа |
| **Min CPM** | минимум floor/CPM в срезе |
| Бюджет | spend |
| Конверсии | post-click/view |
| View rate / quartile 25–100% | video completion |
| Bid responses | наши ответы |

**Вывод:** GetIntent близже всего к целевому продукту: именно **available inventory + Min CPM + CPM + uniques + slices**. Часовой пояс — first-class control.

### 2.3 u-Slicer (IPONWEB) — возможности работы с данными

Документация (рабочие зеркала):  
- https://docs.uslicer.iponweb.com/latest/users-guide/adjusting-time-zone-offset/  
- https://docs.uslicer.iponweb.com/v2/api-v2-reference/query-method/  
- https://docs.uslicer.iponweb.com/v2/api-v2-reference/info-method/  
- https://docs.uslicer.iponweb.com/v2/api-v2-reference/time_series-method/

**Что перенять в нашу систему:**

| Возможность | Детали | Как применить у нас |
|---|---|---|
| Timezone offset | UTC−12…+12 в UI и API (`timezone: N`) | Хранить события в UTC; в API/UI принимать `tz_offset` / IANA и сдвигать `toDate(ts + offset)` |
| Relative periods | `today`, `yesterday`, `-Nd`, `-Nm`, month first/last | Аналогичные пресеты в нашем query layer |
| `split_by` | произвольная комбинация key fields | Любая комбинация измерений витрины |
| Filters | equals / contains / beginswith + AND/OR template | ClickHouse WHERE + expression builder |
| Data filters | пороги по метрикам (`imps > X`) | HAVING / prewhere на агрегатах |
| `info` | каталог key/data fields + descriptions | Schema registry / OpenAPI для агента и UI |
| `time_series` | графики по granularity | Отдельный endpoint поверх daily/hourly |
| Currency conversion | daily FX для cost-полей | Для RUB/USD CPM — словарь курсов по дню |
| Confidence / compressed | оценка точности сжатых данных | Для HLL — документировать error bound |
| Export | csv/xlsx/tsv | `FORMAT CSVWithNames` / выгрузки |

**Не копировать слепо:** u-Slicer — внешний cube; у нас преимущество — доступ к **сырому bid-path DSP** и своим каналам/сегментам.

### 2.4 Наша DSP / ClickHouse (as-is)

| Объект | Наблюдение |
|---|---|
| `click00` / `default.RImpression` | AggregatingMergeTree, ~3.4B rows; есть `uid`, `bid_floor`, `bid_price`, `win_price`, `publisher_id`, `tag_id`, `ssp_tag_id`, `size_id`, `geo_ch`, `url`, device… |
| PARTITION | `sipHash64(request_id) % 100` — **плохо для time-lifecycle** и дневных отчётов |
| ORDER BY | `request_id` — **не подходит** для фильтров по day/geo/ssp |
| Bid request raw | парсятся в `RequestInfoFiller` (полный OpenRTB: site/app/device/user/imp/video/native/pmp…) |
| Вывод | RImpression — хороший **факт выкупа**, но **не** витрина планирования; нужна отдельная daily cube + пайплайн с bid-request уровня |

OpenRTB поля, которые DSP уже умеет читать (фрагмент):  
`imp.bidfloor`, `imp.bidfloorcur`, `imp.tagid`, `imp.secure`, `imp.pmp.deals.*`, `imp.banner|video|native.*`, `site.*`, `app.*`, `device.geo.*`, `device.devicetype|os|osv|ua|ifa|carrier|language`, `user.id|buyeruid|gender|yob|data.segment.*`.

---

## 3. Продуктовые требования

### 3.1 Пользовательские сценарии

1. **Медиапланнер:** «Сколько bid request / uniques / min floor по RU + desktop + 300×250 + domain X за 30 дней?»
2. **Трейдер:** «Где дешёвый inventory (низкий min/avg CPM) с достаточным объёмом?»
3. **Аналитик SSP:** «Какой SSP даёт рост auctions WoW по app vs web?»
4. **ИИ-агент:** строит SQL по каталогу измерений, без full scan сырых логов.

### 3.2 Обязательные метрики (daily)

| Метрика | Определение | Агрегация |
|---|---|---|
| `bid_requests` | число impression opportunities / auctions (imp-level) | `sum` |
| `uniq_users` | approx unique users | `uniqCombined64` / `uniqHLL12` merge |
| `imps` | подтверждённые показы (win + impression pixel) | `sum` |
| `min_bidfloor_cpm` | min `imp.bidfloor` в валюте отчёта (если пришёл) | `min` (игнорировать NULL/0 по политике) |
| `avg_win_cpm` | avg clearing/win price × 1000 / imps или avg по win events | weighted avg |
| *(рекомендуемые)* | `bids`, `wins`, `win_rate`, `avg_bidfloor`, `p50/p90_bidfloor`, `spend`, `avg_viewability` | — |

Валюта: хранить **native** (`bidfloor_cur`) + **normalized RUB** (и опционально USD) через daily FX.

### 3.3 Каталог измерений (target — v1)

Приоритет **P0** (must) / **P1** / **P2**.

#### Время
| Field | P | Notes |
|---|---|---|
| `event_date` | P0 | день в выбранной TZ при query; хранение UTC date + hour |
| `event_hour` | P1 | для heatmap / dayparting forecast |

#### Supply / placement
| Field | P | OpenRTB |
|---|---|---|
| `ssp_id` / `colo_id` | P0 | наш идентификатор источника |
| `inventory_type` | P0 | `site` \| `app` \| `unknown` |
| `publisher_id` | P0 | |
| `domain` | P0 | normalized host |
| `tld` | P1 | |
| `app_id` / `app_bundle` / `app_name` | P0 | |
| `tag_id` / `ssp_tag_id` | P0 | |
| `iab_cat` | P1 | LowCardinality / Array |
| `page_lang` / `device_lang` | P1 | |

#### Geo
| Field | P |
|---|---|
| `country` | P0 |
| `region` | P0 |
| `city` | P1 |
| `zip` | P2 |
| `has_geo` | P1 |

#### Device / env
| Field | P |
|---|---|
| `device_type` | P0 |
| `os` / `os_version` | P0 / P1 |
| `browser` / `browser_version` | P1 |
| `connection_type` | P1 |
| `carrier` | P1 |
| `has_device_id` | P1 |
| `make` / `model` | P2 |

#### Ad format
| Field | P |
|---|---|
| `ad_format` | P0 | banner/video/native |
| `size` | P0 | `WxH` или size_id |
| `position` | P1 |
| `instl` / `rewarded` / `skippable` | P1 |
| `video_placement` / `linearity` / `startdelay` | P1 |
| `secure` | P1 |

#### Deals / auction
| Field | P |
|---|---|
| `auction_type` | P2 | `at` |
| `deal_id` | P1 | PMP |
| `private_auction` | P2 |

#### Audience (осторожно с cardinality)
| Field | P |
|---|---|
| `user_known` | P0 |
| `gender` / `age_bucket` | P1 |
| `segment_ids` | P2 | Array; отдельная exploded-витрина |

**Анти-паттерн:** класть все измерения в одну wide-строку с page_url + keywords + segments — взорвёт cardinality. См. §5–§6.

---

## 4. Архитектура решения

```
┌─────────────────┐     ┌──────────────────┐     ┌────────────────────────────┐
│ FE Bid path     │────▶│ Raw / stage logs │────▶│ CH: inventory_events_*     │
│ OpenRTB parse   │     │ (Kafka/file/LP)  │     │ (MergeTree, TTL 7–14d)     │
└─────────────────┘     └──────────────────┘     └─────────────┬──────────────┘
                                                               │ MV / batch
                                                               ▼
                                                    ┌────────────────────────────┐
                                                    │ inventory_daily_cube       │
                                                    │ sample_rate + L0 control   │
                                                    │ AggregatingMergeTree 45d   │
                                                    └─────────────┬──────────────┘
                                                               │
                         ┌─────────────────────────────────────┼────────────────┐
                         ▼                                     ▼                ▼
                   Query API / SQL                      Forecast jobs      Agent tools
                   (tz, filters, split)                 (Prophet/ETL)      (info+query)
```

### 4.1 Слои данных

1. **Events (optional short retention)** — 1 row ≈ 1 imp opportunity (+ late win/imp updates).  
2. **Daily cube (primary product table)** — pre-agg по **базовому ключу измерений P0**.  
3. **Specialized cubes** — отдельно: `by_segment`, `by_hour`, `by_deal` (чтобы не раздувать основную).

### 4.2 Источник фактов (важно для живой DSP)

| Факт | Откуда брать |
|---|---|
| Bid request / auction | момент приёма OpenRTB (до/после фильтров — **явно зафиксировать политику**: raw ingress vs post-eligibility) |
| Bidfloor | `imp.bidfloor` (+ currency) |
| Unique user | `user.buyeruid` / `user.id` / cookie / ifa (единый `uid_hash`) |
| Impression | существующий impression log / `RImpression` |
| Win CPM | `win_price` / clearing price из win notice |

**Рекомендация политики v1:**  
`bid_requests` = auctions, дошедшие до bidder decision (post parse, pre или post campaign filter — выбрать одно и задокументировать). Для «доступного инвентаря как у GetIntent» ближе **post-parse all seen inventory**, а не только то, на что ответили bid.

---

## 5. Затраты, сэмплирование и оптимизации точности

Индустриальная практика (Hybrid / GetIntent / u-Slicer): **полный raw bid-stream на 45 дней не хранят**; хранят сэмпл событий +/или pre-agg кубы, а метрики масштабируют на `1/sample_rate`. u-Slicer даже отдаёт `confidence_range` для compressed datasets.

### 5.1 Оценка затрат (на базе наших фактов click00)

Наблюдения на `default.RImpression` (2026-11-24):

| Показатель | Значение | Следствие |
|---|---|---|
| Размер таблицы | **~2.52 TiB**, ~3.43B rows | long-term event store дорог |
| Imps / день (21–23.11) | **~8.3–8.7M** | это уже *выкуп*, не весь ingress |
| Uniq users / день | **~1.85–1.96M** | HLL обязателен, exact uniq не нужен |
| Domains / день (sample) | **~24k** | domain в ключе куба — ок; page_url — нет |
| Full scan 3 дня (count+uniq+min+avg) | **~4+ мин** | без pre-agg UI/агент невозможны |
| Hybrid demo scale | **~1.8B auctions / hour** | верхняя оценка отрасли; наш ingress ≫ wins |

**Модель объёма куба (не raw):**

Пусть `C` = число комбинаций измерений в день (cardinality ключа), `D = 45`.

| Сценарий ключа | Оценка C / день | Строк за 45d | Порядок размера* |
|---|---:|---:|---|
| L0: date×ssp×type×country×device×format×size | 10³–10⁵ | ≤ 5M | единицы–десятки GB |
| L1: + domain/app/tag/region | 10⁶–10⁷ | 50M–500M | десятки–сотни GB |
| L1 + hour | ×24 | миллиарды cells | **недопустимо без семпла/top-N** |
| Raw bid events 100% × 45d | ≫ imps (часто 10–100×+) | триллионы потенциально | **TiB+/неприемлемо** |

\*при AggregateFunction states + сжатии; exact зависит от codec/merge.

**Доп. затраты помимо диска:** CPU merges AggregatingMergeTree, память на HLL-states, сеть ingest с FE, нагрузка на bid-path если семплировать синхронно (запрещено — только async).

**Вывод:** хранить нужно **не честный 100% event log на 45 дней**, а:

1. короткий raw/stage (дни) опционально;  
2. daily cube с контролируемой кардинальностью;  
3. **управляемый sample rate** на уровне ingress → куб, с экстраполяцией метрик.

### 5.2 Управляемый уровень сэмплирования (обязательное требование)

#### 5.2.1 Конфиг (единая точка управления)

```yaml
# inventory_sampling.yaml (или CH dict / ZooKeeper / env)
default_sample_rate: 0.02          # 2% — выбранный стартовый default
tiers:
  overview:   { rate: 0.02, dims: [L0] }      # дешёвые дашборды
  planning:   { rate: 0.02, dims: [L1] }      # основной planning
  deep_dive:  { rate: 0.10, dims: [L1], scope: top_slices_only }
  full_audit: { rate: 1.00, dims: [L0], ttl_days: 3 }  # краткий 100% для сверки
# Стратификация: не меньше N событий на страту, иначе boost rate
stratify_by: [ssp_id, country, inventory_type]
min_events_per_stratum_per_hour: 50
# Детерминизм
hash_key: request_id               # или uid_hash
# Kill-switch
enabled: true
max_cpu_pct_on_fe_host: 5          # семпл только off-path / log tail
```

Смена `default_sample_rate` **не требует** пересоздания схемы: rate пишется в каждую строку куба / метаданные дня; query layer всегда делает `metric / sample_rate` (см. ниже). Допускается **разная rate по дням** (история конфига).

#### 5.2.2 Алгоритм отбора (детерминированный, без bias по времени)

```text
keep = cityHash64(hash_key) % 1_000_000 < sample_rate * 1_000_000
```

- Один и тот же `request_id` всегда in/out → стабильные uniques при rescale.  
- **Стратификация:** для редких страт (малый SSP × geo) повышать локальный rate до `min_events_per_stratum`, иначе long-tail «пропадает».  
- **Wins/imps path:** рекомендуется **rate=1.0** (объём уже ~8M/день — терпимо) или отдельный полный куб wins; семплировать в первую очередь **bid_requests / available inventory**.

#### 5.2.3 Экстраполяция метрик (обязательно в view/API)

| Метрика | Как считать из семпла |
|---|---|
| `bid_requests` | `sum(sampled_count) / sample_rate` |
| `imps` / `spend` / sums | то же, **если** они тоже сэмплировались; если wins full — без деления |
| `avg_*` (win CPM, bidfloor) | **без** деления на rate (средние несмещённые при SRS) |
| `min_bidfloor` | **смещён** при семпле (min завышается); см. §5.2.5 |
| `uniq_users` | HLL по семплу → rescale **нелинейный**; лучше HLL на full stream sketches или `uniq / f(rate)` с калибровкой |

В ответе API всегда отдавать:

```json
{
  "sample_rate": 0.02,
  "sample_tier": "planning",
  "is_extrapolated": true,
  "confidence_range_pct": 3.5
}
```

(аналог u-Slicer `confidence_range`).

#### 5.2.4 Поля схемы для управления семплом

В куб добавить:

- `sample_rate Float32` — rate, применённый к этой строке/дню  
- `sample_tier LowCardinality(String)` — `overview|planning|deep_dive|full_audit`  
- `sampled_bid_requests` / states — **сырые** счётчики до экстраполяции  
- опционально `population_bid_requests` — если параллельно есть exact counter с L0 (control totals)

**Контрольные totals (рекомендуется):** отдельная маленькая таблица `inventory_control_daily` с **100%** агрегатами только по L0 (ssp×country×type×format, без domain). По ней калибруем и ловим drift семпла:

```text
scale_correction = control.bid_requests / extrapolated_L0.bid_requests
```

#### 5.2.5 Особые метрики при семпле

| Метрика | Проблема | Решение без «угадывания» |
|---|---|---|
| `min_bidfloor` | семпл завышает min | t-digest / `quantilesDD` → p01/p05 как «практический floor»; exact min только на `full_audit` или control |
| `uniq_users` | rescale смещён | хранить `uniqCombined64`/HLL state; для report использовать calibrated factor с control; или HyperLogLog на **всех** событиях в FE/LP (sketch merge дешевле raw) |
| Rare slices | нули/шум | stratified boost + UI flag `low_confidence` если `sampled_n < threshold` |

#### 5.2.6 Операционное управление rate

| Действие | Как |
|---|---|
| Поднять точность на неделю | `deep_dive` 20% + TTL; не трогать историю |
| Авария диска / merges | drop rate до 0.5–1%; control L0 остаётся 100% |
| Сверка с GetIntent | включить `full_audit` 100% на 1–3 дня L0/L1-top |
| A/B качества | два tier параллельно за один день, сравнить MAPE vs control |

Конфиг читается ingest-job'ом каждый N минут; **изменение rate не ретроактивно** пересчитывает старые дни (кроме явного backfill job).

### 5.3 Оптимизации **без ущерба точности** (не семпл)

Их применять **раньше или вместе** с семплом: они уменьшают объём, сохраняя exact (или почти exact для additive metrics).

| # | Приём | Эффект | Точность |
|---|---|---|---|
| 1 | **Pre-agg daily cube** вместо raw 45d | диск/CPU ↓ на порядки | exact для sum/count в ключе |
| 2 | **Уровни L0/L1/L2** (rollup + detail) | L0 exact и крошечный; L1 только нужные dims | exact на своём ключе |
| 3 | **Top-N + `__other__`** по domain/tag (остальное в bucket) | cardinality ↓ 10–100× | exact totals; detail только по хвосту top |
| 4 | **Отдельные кубы** (hour / segment / deal), не одна wide | нет декартова взрыва | exact |
| 5 | **HLL / uniqCombined sketches** вместо хранение uid | state ~KB vs миллионы uid | approx с известной ошибкой (~1–2%) |
| 6 | **t-digest / quantiles** для floor distribution | exact min не нужен для планирования | лучше для «с какого CPM входить» |
| 7 | **LowCardinality + codec** (`ZSTD`, `Delta` для dates) | сжатие | bit-exact |
| 8 | **PARTITION by month + TTL 45d** | дешёвый DROP | bit-exact |
| 9 | **Projections** в CH на частые ORDER BY | ускорение query без дублирования логики | bit-exact |
| 10 | **Dictionaries** для ssp/publisher names | меньше String в фактах | bit-exact |
| 11 | **Control totals 100% L0** | калибровка + anomaly | exact на L0 |
| 12 | **Incremental MV** только на append | нет nightly full reagg | exact merge states |
| 13 | **Не писать page_url/keywords в куб** | главный anti-bloat | n/a |
| 14 | **Wins full / bids sampled** (разные rate) | семпл там, где объём | wins exact |
| 15 | **Sketch merge на FE/LP** (count+HLL+sum floor) без raw row | почти без потерь add. метрик | exact sum/count; HLL approx |

**Предпочтительный стек v1:**  
`(14)+(1)+(2)+(5)+(8)+(11)+(13)` → семпл bid_requests @ **2%** + full wins + L0 control 100%. Это обычно даёт **acceptable planning accuracy** при десятках GB, а не мульти-TiB raw.

### 5.4 Оценка объёма куба при sample_rate = 2%

База с `RImpression` за вчера (wins-only, полный объём):

| Метрика | Значение |
|---|---:|
| Imps / день | ~8.35M |
| Distinct key ≈ colo×pub×tag×size×device×domain×ssp_tag | **~581k / день** |
| Domains / день | ~19k |
| Avg row RImpression on disk | ~808 B (для сравнения raw) |

Куб хранит **1 строка на комбинацию измерений на день** (после merge), не 1 строку на event. Поэтому размер куба ≈ `Σ keys/day × bytes/key_state × 45`, а rate=2% влияет так:

1. режет число **входных** events (CPU/ingest);  
2. слегка снижает keys за счёт отрезания long-tail;  
3. **не** уменьшает keys, которые приходят с **full wins** (если imps в той же таблице) — нижняя граница ≈ keys wins.

**Нижняя граница (только wins-driven keys, rate imps=100%):**

```
581k keys/day × 45 ≈ 26M строк
× ~0.5–2 KiB/строка (HLL + t-digest + dims, после ZSTD)
≈ 13–50 GiB на диске за 45 дней
```

**С bid_requests @ 2%** keys = union(wins keys, sampled-bid keys).  
Sampled bids добавляют домены/таги, на которые не выиграли. Оценка от неизвестного win-rate ingress:

| Если bid_requests / imps | Events @2%/день | Ожидаемые keys/день | Строк ×45d | Диск (оценка) |
|---|---:|---:|---:|---|
| ×10 (~85M bids) | ~1.7M | ~0.6–1.0M | ~30–45M | **~20–70 GiB** |
| ×50 (~420M) | ~8.5M | ~1.0–2.0M | ~45–90M | **~40–120 GiB** |
| ×100 (~850M) | ~17M | ~1.5–3.0M | ~70–130M | **~60–180 GiB** |

Пока точный QPS bid ingress не замерен — **рабочий ориентир v1: 30–100 GiB** на основную planning-таблицу при 2% + full wins, без hour/segments.

Control L0 (100%, грубый ключ): ≪ **1 GiB** за 45d.

**Не путать с raw:** 45d × 8.5M imps ≈ 380M rows × 808 B ≈ **~300 GiB только wins raw**; full bids без семпла легко уходят в **TiB+**. Куб при 2% дешевле на порядок+.

### 5.5 Одна большая таблица vs несколько

**Цель — одна основная planning-таблица: да, так и делаем.**

| Объект | Решение | Почему |
|---|---|---|
| `inventory_planning_daily` (L1, day) | **одна большая** | все срезы planning в одном `GROUP BY` / `split_by`; один API; проще агенту |
| `inventory_control_daily` (L0, 100%) | **отдельная маленькая** | другой grain + другой rate; смешивать в L1 вредно для ORDER BY и drift-калибровки |
| Hour / segments / page_url | **не класть в основную** | иначе cardinality ×24 или ×segments → сотни GiB–TiB и «одна большая» перестаёт быть управляемой |

Почему одна L1 работает в ClickHouse:

- `AggregatingMergeTree` как раз для wide rollup-ключей;  
- фильтры по префиксу `ORDER BY (event_date, ssp, …)` prune хорошо;  
- `sample_tier` / `sample_rate` — колонки внутри той же таблицы, не отдельные таблицы на каждый rate.

Когда всё же дробить (только если упрёмся в бюджет §5.6):

1. вынести top-N domain detail в `…_domain_top` + `__other__` в основной;  
2. или projection / вторая MV с урезанным ключом — не второй source of truth для API.

**Итог:** одна большая daily L1 + крошечный control L0. Не плодить «на каждый срез таблицу».

### 5.6 Бюджеты и алерты стоимости

| Бюджет | Порог (стартовый, уточнить на canary) |
|---|---|
| Размер `inventory_planning_daily` | alert > **100 GiB**, hard > **200 GiB** |
| Rows / day L1 | alert > **3M** |
| Merge time / part count | по `system.parts` |
| Extrapolation drift vs control L0 | alert если \|1 − scale_correction\| > **3%** |
| Sampled_n на строку отчёта | UI warning если < 100 |

### 5.7 Рекомендованные дефолты v1

| Поток | Sample rate | Комментарий |
|---|---:|---|
| Bid requests → L1 planning cube | **2%** (0.02) | выбранный default |
| Bid requests → L0 overview | counters в control **100%** | без семпла |
| Impressions / wins / spend | **100%** | ~8–9M/день |
| Control L0 | **100%** | калибровка |
| Hour detail | **не в v1** / иначе 1% + top SSP | иначе раздувает «одну» таблицу |
| Segments L3 | отложить | иначе отдельная таблица позже |

Формула в продукте: пользователь видит **extrapolated** числа; в debug — `sampled` + `rate` + `confidence`.

---

## 6. Схема ClickHouse (целевая)

Правила (ClickHouse Best Practices):  
- `schema-pk-plan-before-creation` — ORDER BY проектировать до создания  
- `schema-pk-cardinality-order` — low → high cardinality  
- `schema-pk-prioritize-filters` — частые фильтры в префиксе ключа  
- `schema-types-lowcardinality` / `minimize-bitwidth` / `avoid-nullable`  
- `schema-partition-lifecycle` — partition по времени для TTL 45d  
- `query-mv-incremental` — инкрементальные MV в AggregatingMergeTree  

### 6.1 Таблица-куб (основная)

```sql
CREATE TABLE default.inventory_planning_daily
(
  event_date Date,
  sample_tier LowCardinality(String) DEFAULT 'planning',
  sample_rate Float32 DEFAULT 0.02,
  -- dimensions (P0/P1)
  ssp_id LowCardinality(String) DEFAULT '',
  inventory_type LowCardinality(String) DEFAULT 'unknown', -- site|app|unknown
  country FixedString(2) DEFAULT '',
  region LowCardinality(String) DEFAULT '',
  device_type LowCardinality(String) DEFAULT '',
  os LowCardinality(String) DEFAULT '',
  ad_format LowCardinality(String) DEFAULT '',
  size LowCardinality(String) DEFAULT '',
  publisher_id String DEFAULT '',
  domain String DEFAULT '',
  app_bundle String DEFAULT '',
  tag_id String DEFAULT '',
  user_known UInt8 DEFAULT 0,
  deal_id String DEFAULT '',
  bidfloor_cur LowCardinality(String) DEFAULT 'RUB',

  -- sampled additive states (extrapolate in view by / sample_rate)
  bid_requests_smpl AggregateFunction(sum, UInt64),
  uniq_users_smpl AggregateFunction(uniqCombined64, UInt64),
  -- wins path: prefer sample_rate=1 for these (still stored as states)
  imps AggregateFunction(sum, UInt64),
  bidfloor_sum AggregateFunction(sum, Float64),
  bidfloor_cnt AggregateFunction(sum, UInt64),
  bidfloor_q AggregateFunction(quantilesTDigest(0.01, 0.05, 0.5), Float64),
  win_price_sum AggregateFunction(sum, Float64),
  win_cnt AggregateFunction(sum, UInt64),
  spend_sum AggregateFunction(sum, Float64),
  sampled_rows AggregateFunction(sum, UInt64)
)
ENGINE = AggregatingMergeTree
PARTITION BY toYYYYMM(event_date)
ORDER BY (
  event_date,
  sample_tier,
  ssp_id,
  inventory_type,
  country,
  device_type,
  ad_format,
  size,
  os,
  region,
  domain,
  app_bundle,
  publisher_id,
  tag_id,
  user_known,
  deal_id,
  bidfloor_cur,
  sample_rate
)
TTL event_date + INTERVAL 45 DAY DELETE
SETTINGS index_granularity = 8192;
```

Отдельно (маленькая, **rate=1**):

```sql
CREATE TABLE default.inventory_control_daily
(
  event_date Date,
  ssp_id LowCardinality(String),
  inventory_type LowCardinality(String),
  country FixedString(2),
  ad_format LowCardinality(String),
  bid_requests UInt64,
  uniq_users AggregateFunction(uniqCombined64, UInt64)
)
ENGINE = SummingMergeTree
PARTITION BY toYYYYMM(event_date)
ORDER BY (event_date, ssp_id, inventory_type, country, ad_format)
TTL event_date + INTERVAL 45 DAY DELETE;
```

### 6.2 Query view (экстраполяция + confidence)

```sql
CREATE VIEW default.v_inventory_planning_daily AS
SELECT
  event_date, sample_tier, sample_rate,
  ssp_id, inventory_type, country, region, device_type, os,
  ad_format, size, publisher_id, domain, app_bundle, tag_id, user_known,
  deal_id, bidfloor_cur,
  toUInt64(sumMerge(bid_requests_smpl) / sample_rate) AS bid_requests,
  uniqCombined64Merge(uniq_users_smpl) AS uniq_users_sampled,
  -- продукт может применять calibrated_uniq через control; см. §5.2.5
  sumMerge(imps) AS imps,
  if(sumMerge(bidfloor_cnt) = 0, NULL,
     sumMerge(bidfloor_sum) / sumMerge(bidfloor_cnt)) AS avg_bidfloor_cpm,
  quantilesTDigestMerge(0.01, 0.05, 0.5)(bidfloor_q) AS bidfloor_p01_p05_p50,
  if(sumMerge(win_cnt) = 0, NULL,
     sumMerge(win_price_sum) / sumMerge(win_cnt)) AS avg_win_cpm,
  sumMerge(spend_sum) AS spend,
  sumMerge(sampled_rows) AS sampled_rows,
  if(sumMerge(sampled_rows) < 100, 1, 0) AS low_confidence
FROM default.inventory_planning_daily
GROUP BY ALL;
```

> `min_bidfloor` в UI v1 = `bidfloor_p01` (или p05), не `min()` по семплу. Exact min — только `full_audit` / отдельный job.

### 6.3 Стратегия cardinality (обязательно)

| Уровень | Что в ключе | Ожидание |
|---|---|---|
| L0 daily rollup | P0 без domain/tag | быстрые overview; **лучше 100% control** |
| L1 planning | P0 + domain/app/tag | основной UI; sample **2%** |
| L2 | + hour | dayparting; только sample + top-N |
| L3 | segments exploded | отдельная таблица |

Для domain/app высокой кардинальности — `bloom_filter` / `tokenbf_v1` skipping index по `domain` (`query-index-skipping-indices`) + **top-N + `__other__`**.

### 6.4 Ingest

- Батчи 10k–100k (`insert-batch-size`).  
- Не делать `ALTER UPDATE` по кубу (`insert-mutation-avoid-update`).  
- Late impressions: повторный insert aggregate states (AggregatingMergeTree сомёржит).  
- Не строить отчёты напрямую с `RImpression` (неверный ORDER BY/PARTITION).  
- Семплирование **только** в async ingest/LP; на hot bid-path — максимум lock-free sketch/counter.  
- `sample_rate` брать из актуального конфига (§5.2.1) и писать в строку.

---

## 7. Прогнозы (forecast)

Цель: не только «что было», но «что будет» на горизонте 1–14 дней.

### 7.1 Базовые модели (v1, без ML-инфры)

| Модель | Когда | Как |
|---|---|---|
| **Same-day-last-week (SDLW)** | сильная недельная сезонность RTB | `value(d) ≈ value(d-7)` с коэффициентом тренда |
| **Trailing 7/14/28 avg** | стабильные срезы | скользящее среднее по `bid_requests` / `uniq_users` |
| **Day-of-week profile** | media plan | доля DoW × weekly total |
| **Floor-aware volume** | «если поднимем CPM» | кривая: histogram bidfloor → cum volume (из raw/events sample) |

Хранение прогноза:

```sql
-- пример
forecast_date Date,
as_of Date,           -- когда посчитан
model LowCardinality(String),
-- те же dimensions, что в кубе (или hash slice_id)
pred_bid_requests Float64,
pred_uniq_users Float64,
pred_avg_win_cpm Float64,
confidence_low Float64,
confidence_high Float64
```

### 7.2 Продвинутые (v2)

- Prophet / NeuralProphet / TFT на top-N срезах (SSP×country×format).  
- Иерархический reconcile (country → region → city).  
- Win-rate × available auctions → expected imps при заданном bid.  
- Учёт праздников RU/KZ и крупных app-релизов.

### 7.3 UX прогноза (как у Hybrid «plan campaign»)

Вход: filters + period future + budget/CPM cap.  
Выход: predicted auctions, reach, expected imps, recommended bid ≥ pX floor, confidence.  
Прогноз строить по **extrapolated** рядам; в метаданных указывать `sample_tier` исходных данных.

---

## 8. API / контракт для ИИ-агента

Минимальный набор методов (по аналогии с u-Slicer):

### `POST /inventory/info`
Каталог dimensions + metrics + timezone support + `first_date`/`last_date` + **доступные `sample_tier` / default rates**.

### `POST /inventory/query`
```json
{
  "start_date": "-45d",
  "end_date": "yesterday",
  "timezone": 3,
  "sample_tier": "planning",
  "split_by": ["event_date", "ssp_id", "domain"],
  "filters": [
    {"name": "inventory_type", "match": "equals", "value": ["site"]},
    {"name": "country", "match": "equals", "value": ["RU"]}
  ],
  "data_fields": [
    "bid_requests", "uniq_users", "imps", "bidfloor_p05", "avg_win_cpm"
  ],
  "order_by": [{"name": "bid_requests", "direction": "DESC"}],
  "limit": 100
}
```

Ответ обязан включать `sample_rate`, `is_extrapolated`, `confidence_range_pct`, `low_confidence` (см. §5.2.3).

### `POST /inventory/time_series`
График одной метрики с `granularity=day|hour`.

### `POST /inventory/forecast`
Filters + horizon + model.

### `POST /inventory/sampling` (ops)
Get/set desired rates по tier (с audit log); не ретроактивно без `backfill=true`.

Агент **обязан**: сначала `info`, потом query с LIMIT; не сканировать raw events без date predicate; не запрашивать `full_audit` без явной нужды.

---

## 9. План реализации для ИИ-агента (пошагово)

### Phase 0 — Discovery (1–2 дня)
1. Зафиксировать политику «что считаем bid_request».  
2. Найти/создать сырой поток OpenRTB dimensions (лог FE / LP / Kafka).  
3. Сопоставить поля `RequestInfoFiller` → колонки куба.  
4. Проверить наличие win_price / bid_floor на всём пути.  
5. Замерить QPS bid ingress и подтвердить объём при rate=**0.02** (ориентир §5.4).

### Phase 1 — MVP куб (3–5 дней)
1. DDL `inventory_planning_daily` + `inventory_control_daily` (+ MV или nightly batch).  
2. Ingest с **конфигурируемым семплом** (§5.2) и экстраполяцией во view.  
3. Backfill 45 дней (если есть сырьё; иначе с сегодня + накопление).  
4. View + 5 эталонных SQL (как GetIntent yesterday web).  
5. Сверка extrapolated L0 vs control (drift < 3%).

### Phase 2 — Query layer (2–3 дня)
1. `info` / `query` / timezone / sample_tier metadata.  
2. Защита: max split cardinality, max rows, mandatory date range, low_confidence flag.  
3. Ops endpoint/конфиг для смены rate.  
4. Документация для агента в `docs/`.

### Phase 3 — Forecast v1 (2–4 дня)
1. SDLW + DoW profile offline job.  
2. Таблица `inventory_forecast_daily`.  
3. Простая оценка MAPE на holdout 7 дней.

### Phase 4 — UI / agent polish
1. Пресеты срезов как в Hybrid (Placement/Geo/Device/Banner).  
2. Export CSV (+ колонки sample_rate/confidence).  
3. Алерты: падение auctions WoW; budget размера куба; drift vs control.

---

## 10. Acceptance criteria

- [ ] За любой день в окне 45d доступны метрики: bid_requests, uniq_users, imps, bidfloor_p05 (или min на full_audit), avg_win_cpm.  
- [ ] Можно комбинировать ≥ 8 измерений P0 в одном запросе без full scan raw.  
- [ ] Timezone offset влияет на границы дня корректно (сверка Europe/Moscow vs UTC).  
- [ ] TTL/partition удаляет данные старше 45 дней без тяжёлых DELETE.  
- [ ] Документирован mapping UI-срезов Hybrid/GetIntent → наши поля → OpenRTB.  
- [ ] Есть `info` каталог для агента.  
- [ ] Forecast v1 отдаёт pred на 7 дней для top-срезов с указанием модели.  
- [ ] Не деградирует прод DSP (ingest только async/batch; без блокировки bid path).  
- [ ] `sample_rate` задаётся конфигом; смена rate отражается в новых днях без DDL.  
- [ ] Ответ query содержит extrapolated metrics + `sample_rate` + confidence/low_confidence.  
- [ ] Control L0 100% существует; \|drift\| extrapolated vs control < 3% на дневных totals.  
- [ ] Размер planning-таблиц в пределах бюджета (§5.6) при rate=2%.

---

## 11. Риски и решения

| Риск | Митигация |
|---|---|
| Взрыв cardinality (domain×tag×city×hour) | уровневые кубы; top-N + others; отдельный hour cube |
| TiB-класс raw / куба | семпл bid_requests + full wins; не хранить 45d raw (§5) |
| Bias семпла / пропавший long-tail | стратификация; control L0; low_confidence |
| Bidfloor часто 0/empty | `bidfloor_present_rate`; p01/p05 вместо min по семплу |
| Uniques переоценены/недооценены | HLL + калибровка; не продавать как exact |
| Расхождение с GetIntent/Hybrid | разные SSP mix и фильтры eligibility — сравнивать только на пересечении источников |
| RImpression непригоден как base | не использовать как planning source of truth |
| PII | хранить только hash uid; не складывать raw ifa в long-term cube |
| Семпл на hot path | только async; FE — sketches/counters |

---

## 12. Справочник: сводная матрица срезов

| Группа | Hybrid Insights | GetIntent available_inventory | Наш P0 field |
|---|---|---|---|
| Time | Time | Дата/Час/Месяц | `event_date` (+ hour P1) |
| SSP | SSP | SSP | `ssp_id` |
| App/Web | Type | В моб. приложении | `inventory_type` |
| Publisher | Publisher ID | Publisher (ID) | `publisher_id` |
| Domain | Domain | Домен / TLD | `domain` |
| App | App ID/name/cat | Приложение (ID) | `app_bundle` |
| Tag | PlacementId | Tag (ID) | `tag_id` |
| Geo | Country/State/City/ZIP | Страна/Регион/Город/ZIP | `country`,`region`,`city` |
| Device | Device/OS/Browser/… | Тип устройства/ОС/Браузер | `device_type`,`os`,`browser` |
| Carrier | Mobile carrier | Оператор | `carrier` |
| Format | Format/size/video flags | Видео/Native/Size/Rewarded/Skip | `ad_format`,`size`, flags |
| Deals | — | Available/Bid deal | `deal_id` |
| User | Age/Gender/segments | Пользователь известен/Сегменты | `user_known` (+P1 demo) |
| Metrics | Auctions | Доступные показы, Min CPM, CPM, Uniques | см. §3.2 |
| Sampling | compressed + confidence | (не явно в UI) | §5.2 tiers + confidence |

---

## 13. Deliverables агента

1. DDL + MV/batch scripts в `docs/sql/` или миграциях CH.  
2. Mapping table OpenRTB → columns (этот документ §3.3 / §12).  
3. `inventory_sampling.yaml` + код чтения rate / стратификации.  
4. Query API или SQL cookbook (с экстраполяцией и confidence).  
5. Forecast job + backtest note.  
6. Runbook: свежесть куба, смена sample_rate, сверка vs control, бюджеты диска.

---

## 14. Открытые решения (нужно подтверждение владельца продукта)

1. **Определение bid_request:** all parsed ingress vs post-targeting-eligible vs only bid-responded.  
2. **Валюта отчёта по умолчанию:** RUB vs USD.  
3. **Нужен ли hour cube в v1** или только daily.  
4. **Сегменты в v1** или отложить в L3.  
5. Где крутить ingest: только CH batch из логов vs стрим из Kafka.  
6. **Стартовый `default_sample_rate`:** зафиксирован **2% (0.02)**; пересмотр только если drift/бюджет вне порогов.  
7. **Wins всегда 100%?** (рекомендация: да).  
8. Порог `low_confidence` (sampled_rows) и целевой drift vs control (рекомендация: 100 rows / 3%).  
9. Подтвердить отношение bid_requests/imps на проде (для уточнения §5.4).

---

*Документ подготовлен по результатам разбора Hybrid Insights (Dsp Traf dimensions), GetIntent dataset «Доступный трафик», u-Slicer API/timezone docs и текущей схемы ClickHouse/OpenRTB парсера DSP Genius Desk. §5 дополнен оценкой затрат и требованиями к управляемому семплированию (2026-11-24).*
