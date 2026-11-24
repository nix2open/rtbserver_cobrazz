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

- Поле БД `campaigncreativegroup.spend_mode` (`A`/`E`, default `A`)
- Код читает `spend_mode`, протаскивает в конфиг/IDL
- Billing: paced cap **только если** `enable_spend_pacing=true` **и** `spend_mode='E'`
- Runtime/CMS default: `enable_spend_pacing=false`

## Фаза 1 — тёмный релиз (поведение = сегодня)

Согласованно обновить (один релиз / узкое окно):

1. `CampaignServer` (`adbe00`)
2. все `BillingServer` на `adfe*`
3. все `CampaignManager` на `adfe*`

Конфиг Billing: `enable_spend_pacing=false` (или атрибут отсутствует).  
Все CCG остаются `spend_mode='A'`.

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

## Schedule-aware Even (ветка `feature/spend-pacing-schedule-aware`)

Even paced cap считается по **активным окнам** `weekly_run_intervals` (день/часть суток
в TZ рекламодателя), а не по полным 24h. Пустой schedule → прежнее поведение (весь день).

Деплой: binary с обновлённым `DeliveryLimitConfig` IDL → согласованный restart
`be-CampaignServer` + все `fe*-BillingServer` (CM/FCGI/nginx не обязательны).
Смерживать с другими апдейтами в один RPM-релиз.
