# Триггеры `ssptagid` / `rtbssptagid`

FE берёт `imp.tagid` из OpenRTB и кладёт в page keywords две формы:

| Форма | Пример (tagid = `100259`) |
|---|---|
| без source | `rtbssptagid100259` |
| с source | `rtb{source}ssptagid100259` (напр. `rtbbetweenssptagid100259`) |

Небуквенно-цифровые символы в значении нормализуются в `x`, регистр → lower:
`007/android/banner` → `007xandroidxbanner`.

## Как писать триггер в канале (page / type P)

### Вариант 1 — через UI (рекомендуется)

Словарь + `<>` + **сырое** значение tagid:

```text
ssptagid<>100259
ssptagid<>007/android/banner
```

UI/нормализация приводят это к матчу с `rtbssptagid…`. Source в триггере
указывать не нужно — сработает source-less форма с FE.

### Вариант 2 — явное keyword (API / bulk / AutoCTR)

Пишите сразу токен, который эмитит FE **без source**:

```text
rtbssptagid100259
rtbssptagid007xandroidxbanner
```

Значение уже должно быть в нормализованном виде (`x` вместо `/ : _` и т.п.).

## Что не сработает

| Неверно | Почему |
|---|---|
| `ssptagid100259` | нет префикса `rtb` — FE так не эмитит |
| `rtbbetweenssptagid100259` | только один SSP; без source в канале лучше |
| `ssptagid<>007xandroidxbanner` | в `<>` лучше сырой tagid; `x` — уже нормализация FE |

## Макрос в креативе / пикселе

Сырое значение `imp.tagid` доступно как request-token:

| Макрос | Результат |
|---|---|
| `##SSP_TAG_ID=##` | как пришло от SSP, напр. `100259` или `007/android/banner` |
| `##base64:SSP_TAG_ID=##` | то же в base64 (prefix `base64:` — общий механизм TextTemplate, как у `##base64:DOMAIN=##`) |
| `##mime-url:SSP_TAG_ID=##` | URL-encoding при необходимости |

Не путать с `##TAGID=##` (внутренний DSP tag) и `##ETID=##` (`ext_tag_id`).

## Проверка

После правки триггеров дождитесь подхвата ChannelServer (`triggers_version`
канала обновится). Hits канала → `stat.channelinventory`. Наличие tagid в
трафике → `default.RImpression_recent.ssp_tag_id` (это wins, не hits).
