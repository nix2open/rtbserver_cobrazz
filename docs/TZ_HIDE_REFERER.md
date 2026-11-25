# ТЗ: hide_referer на уровне стратегии (CCG)

## Цель

На CCG флагом управлять подменой HTTP Referer при клике: вместо страницы
паблишера на лендинг уходит технический домен платформы.

## Модель данных (CCG)

| Поле | Тип | Default | Смысл |
|------|-----|---------|--------|
| `hide_referer` | `bool` | `false` | Включить подмену Referer на техдомен |

Константа: `ad.new-programmatic.com`.

## Postgres

Обновить таблицу `public.campaigncreativegroup` (как у `spend_mode`, см.
`docs/sql/2026-11-20_ccg_spend_mode.sql`):

1. Добавить колонку `hide_referer boolean`.
2. `DEFAULT false` — **на первом раскате у всех стратегий `false`**
   (поведение без изменений).
3. Backfill `NULL → false`, затем `NOT NULL`.
4. SQL-миграцию положить в `docs/sql/` (например
   `docs/sql/YYYY-MM-DD_ccg_hide_referer.sql`).

После раската фичи и проверки планируется сменить default на `true`
(и при необходимости массово проставить `true` существующим CCG) —
это **отдельный** follow-up SQL, не часть первого релиза.

В загрузчике конфига: `COALESCE(ccg.hide_referer, false)`.

## Правила

```
if hide_referer:
  на редиректе к лендингу Referer/origin = ad.new-programmatic.com
else:
  поведение как сейчас
```

Логи клика (`ClickInfo.referer` / navigation) **не подменять** — туда по-прежнему
реальный inbound referer.

## Реализация на click-path

1. В `CampaignManagerCore::co_get_click_url` по `creative->campaign->hide_referer`
   выставить признак в `ClickResult` / proto `ClickResultInfo`
   (например `hide_referer: bool`).
2. В `ClickFrontend::process_request_` при `hide_referer=true`: на 302
   (или HTML click-template) добавить `Referrer-Policy: origin`
   (и/или `<meta name="referrer" content="origin">`), чтобы браузер отдал
   origin click-хоста (`ad.new-programmatic.com`), а не страницу паблишера.

## Конвейер конфига

DB (`CampaignCreativeGroup.hide_referer`) →
`CampaignConfigDBSource` → `CampaignDef` → IDL `CampaignInfo` /
gRPC `ConfigCampaign` → `CampaignConfigSource` → `Campaign` →
`co_get_click_url` → `ClickFrontend`.

## Acceptance

| Кейс | Ожидание |
|------|----------|
| `hide_referer=false` | Как сейчас |
| `hide_referer=true` | На лендинге Referer origin ≈ `ad.new-programmatic.com` |
| Лог клика | Реальный inbound referer без подмены |
