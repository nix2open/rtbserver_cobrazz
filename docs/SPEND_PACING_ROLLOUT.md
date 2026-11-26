# Even spend pacing — план раскатки

## Цель

Включить intra-day Even pacing (`spend_mode='E'`) без риска для ASAP и без
mixed-IDL canary на один хост.

## Связанные сущности (тест)

| Что | Значение |
|-----|----------|
| flight_id | `37995` |
| ccg_id | `238746` |
| spend_mode сейчас | `A` (ASAP) |
| daily_budget | `14000` |

## Фаза 0 — предохранитель (код) ✅

- Поле БД `campaigncreativegroup.spend_mode` (`A`/`E`/`R`, default `A`)
  - `A` — ASAP
  - `E` — Even с начала окна (catch-up за «пропущенные» часы)
  - `R` — Remaining Even: остаток бюджета на оставшееся время окна (без catch-up)
- Код читает `spend_mode`, протаскивает в конфиг/IDL
- Billing paced cap **только если** `enable_spend_pacing=true` **и** mode `E` или `R`
- Runtime/CMS XSD default: `enable_spend_pacing=false`; colo `programmatic`
  (`adserver.xml` BillingServer `updateParams`) — **`true`** с `cms20261126080004`
- SQL для `R`: `docs/sql/2026-11-25_ccg_spend_mode_remaining.sql`

## Фаза 1 — тёмный релиз (поведение = сегодня)

Согласованно обновить (один релиз / узкое окно):

1. `CampaignServer` (`adbe00`)
2. все `BillingServer` на `adfe*`
3. все `CampaignManager` на `adfe*`

Конфиг Billing (тёмный релиз): `enable_spend_pacing=false`.  
Сейчас в colo `programmatic` флаг **`true`** (`cms20261126080004`).  
Все CCG по умолчанию `spend_mode='A'` (Even/Remaining только точечно).

Soak 30–60 мин:

- нет CORBA/MARSHAL в логах CS / CM / Billing
- config / delivery-limit update ок
- bid/confirm latency и error rate ≈ baseline
- budget aggregates стабильны

Откат: согласованный rollback RPM этих трёх ролей.

## Фаза 2 — canary логики (один flight/CCG)

1. Убедиться, что фаза 1 зелёная.
2. Включить флаг (через configs / `updateParams/@enable_spend_pacing=true`,
   regenerate Billing configs, restart BillingServer).  
   Можно сначала на одном colo/наборе adfe, если так удобнее оперировать.
3. Перевести тестовую стратегию:

```sql
UPDATE public.campaigncreativegroup
SET spend_mode = 'E'
WHERE ccg_id = 238746;
```

Мониторинг по `238746`: spend vs ideal, ранний упор в paced cap, добор daily.

Откат за секунды:

```sql
UPDATE public.campaigncreativegroup
SET spend_mode = 'A'
WHERE ccg_id = 238746;
```

и/или `enable_spend_pacing=false`.

## Фаза 3 — расширение

Несколько Even CCG → шире. ASAP по умолчанию не трогать.

## Важно

- Canary **не** «один сервер с новым IDL» — только согласованный CS+CM+Billing.
- Canary логики — **на CCG/flight**, не на хосте.

## Schedule-aware + Remaining Even (`feature/spend-mode-remaining-even`)

Paced cap считается по **активным окнам** `weekly_run_intervals` сегодня.
Пустой schedule → линейно по календарным суткам рекламодателя (`now + time_offset`).

**TZ:** интервалы из CS уже сдвинуты в тот же clock, что CM `check_campaign_time_`
(UTC gm на сервере). Для непустого schedule Billing **не** прибавляет `time_offset`
к `now` при расчёте elapsed — иначе на MSK(+3) открытие окна выглядит как
«уже прошло 3 часа» и ловится ложный catch-up.

| Mode | Формула (идея) |
|------|----------------|
| `E` | `budget × (elapsed + lookahead) / open` — catch-up с начала окна |
| `R` | `spent + (budget − spent) × lookahead / remaining` — без catch-up |

`R` удобен для позднего старта и малых бюджетов (например 500 ₽ / 8 ч).

Деплой: CS + Billing (+ CM если тянется общим RPM). Сначала SQL constraint `A|E|R`,
затем binary, флаг `enable_spend_pacing` уже может быть `true`.
