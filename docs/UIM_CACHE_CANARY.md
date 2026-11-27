# UIM: второй тест, только кэш профилей

Инструкция для агента. Первый тест (`docs/UIM_STABILITY_ROLLOUT.md`) на `adfe103`
провален и откачен: вместе с кэшем были урезаны flush/compaction и поднят
`max_base_profile_waiters` до 32. Match ушёл в ~1.5 с, `match_in_progress` в тысячи,
fcap почти встал. Откат вернул `rdb_cache_limit=0` и живой match.

Этот тест меняет **один** атрибут. Ферму не раскатывать. Бинарь не собирать.
Коммит и push — только если человек попросил.

Незакоммиченный diff первого теста не использовать. В
`CMS/Plugin/xslt/UserInfoProfiling/UserInfoManager.xsl` должно остаться
`max_base_profile_waiters="8"`. В `configs_cobrazz` не переносить batching=8,
compaction=8, flush=4 и waiters=32.

## Что меняем

Оба `UserInfoManager` в `configs_cobrazz/adserver.xml` (FrontendSubCluster-1 и -2),
узел `configuration:matchParams`. Добавить только:

```text
rocksdb_cache_size="4294967296"
```

Остальное не задавать и не менять. На хосте до рестарта должно получиться:

| Атрибут | Должно остаться |
|---------|-----------------|
| `rocksdb_batching_threads` | `4` |
| `compaction_threads` | `32` |
| `flush_threads` | `32` |
| `per_compaction_threads` | `4` |
| `max_mem_tables` | `6` |
| `max_base_profile_waiters` | `8` |
| `max_temp_profile_waiters` | `8` |
| `max_freqcap_profile_waiters` | `8` |
| `rocksdb_cache_size` | `4294967296` |

`UserInfoManager.xsl` не трогать. UserBind, FCGI, nginx, CampaignManager не трогать.
`ret_freq_caps` и `silent_match` не менять.

## Хосты

| | |
|--|--|
| Canary | `adfe103` |
| Контроль | `adfe105` |
| cmgr | `nikolay_fedorenko@adbe00`, colo `programmatic` |
| Сервис | `ui-UserInfoManager` (не `fe1-UserInfoManager`) |
| Конфиг | `/opt/foros/server/etc/programmatic/adcluster/adfe103/UserInfoManagerConfig.xml` |
| Статистика | `http://127.0.0.1:10701/stats` |
| SSH | `ssh -i ~/kk1 -o IdentitiesOnly=yes` |

Рестарт с `adbe00`:

```bash
H=adfe103
/opt/foros/manager/bin/cmgr -f programmatic restart ${H}:ui-UserInfoManager
/opt/foros/manager/bin/cmgr -f programmatic -x status ${H}:ui-UserInfoManager
```

Если это имя не находится в `cmgr -x list`, остановиться. Полный `cmgr status` без
`-x` не запускать. Процесс не убивать.

Сборка и публикация RPM — `docs/RTB_RELEASE.md` §4.3 и §4.4, только три config RPM
и уже стоящий `foros-telegraf`. Новый `foros-server-central` в каталог не класть.
`RPM/build.sh` не запускать.

`yum` на canary может упасть на конфликте `rtbserver_ram_fs.conf` с
`foros-config-server-targetrtb`. Тогда три config RPM ставить так:

```bash
rpm -Uvh --replacefiles --oldpackage foros-config-server-programmatic-*.rpm
```

Перед установкой записать `rpm -qa 'foros-config-server-programmatic*'` — это пакет
отката. Откат: те же три RPM с `--oldpackage`, рестарт только `ui-UserInfoManager`.

## Метрики

Окна: база 15 минут без рестарта, после рестарта 5 минут прогрева с момента роста
`match_total`, затем 15 минут. Минуту рестарта в окно не включать. Скорости считать
по двум снимкам `/stats` с паузой 60 секунд, команда снимка — в
`docs/UIM_STABILITY_ROLLOUT.md`, раздел «Метрики». Снять canary и контроль.

Плюс code 8 за те же 15 минут по timestamp, не по сырому хвосту файла. Формат строки:
`Thu DD Mon YYYY HH:MM:SS:usec`. Считать на `adfe100`, `adfe103`, `adfe200` строки
`grpc match failed` и `code=8` с `grpc_endpoint=adfe103:10601` и отдельно с
`adfe105:10601`. Внутри canary разложить на `co_pre_bid_process` и остальные.

Пороги canary против своей новой базы:

| Проверка | Порог |
|----------|--------|
| процесс | `running`, `match_rps` 70–130% базы |
| match | `match_avg_us` ≤ max(1.5 × база, база + 5000) и < 50000 |
| очередь | `match_in_progress` < 200 |
| fcap | `fcap_update_rps` > 0 и 50–150% базы |
| запись | `saves_per_match` 0.8–1.2 |
| кэш | `rdb_cache_limit=4294967296`, `cache_hit_ratio` > 0.3 |
| code 8 canary | ≤ 150% своей базы (рост — откат) |
| code 8 контроль | 50–150% своей базы |
| память | `MemAvailable` > 20 ГБ |

## Порядок

1. Ничего не ставить. Снять базу с `adfe103` и `adfe105` и code 8 за 15 минут.
   Записать цифры. Без записи дальше не идти.
2. Собрать config RPM. До публикации в XML canary проверить таблицу выше.
   Любое отличие от столбца «Должно остаться» — на хост не ставить.
3. Поставить только на `adfe103`. Рестарт только `ui-UserInfoManager`.
   До 10 минут ждать `running`, свежий `service started` в логе UIM и рост
   `match_total`. Строку `Chunk maps loaded` не ждать.
4. Прогрев 5 минут, окно 15 минут, в том же интервале снять `adfe105`.

Откатить сразу, если за 10 минут нет `running`, `match_rps` < 70% базы,
`match_avg_us` ≥ 50000, `match_in_progress` ≥ 200, `fcap_update_rps` = 0 при живом
match, code 8 canary выше своей базы, `MemAvailable` < 15 ГБ или в логе OOM /
RocksDB / нехватка места. После отката проверить `rdb_cache_limit=0` и
`max_base_profile_waiters="8"`.

## Решение

Дальше ферму не ставить.

- Пороги пройдены и `match_avg_us` меньше базы хотя бы на 20% — кэш полезен.
  Написать цифры и остановиться. Раскат только по явной просьбе человека.
- Пороги пройдены, но latency не лучше базы — кэш безопасен и не является рычагом.
  Написать цифры и остановиться. Следующий шаг не конфиг, а отдельное решение
  человека про код из этапа 5 `docs/UIM_STABILITY_ROLLOUT.md` (single-flight и
  пропуск повторной записи базового профиля). В этом окне код не начинать.
- Любой порог провален — откат и стоп.

## Чего не делать

- Не повторять связку первого теста: batching 8, compaction 8, flush 4, waiters 32.
- Не поднимать `max_base_profile_waiters`, `max_freqcap_profile_waiters`,
  `max_temp_profile_waiters`.
- Не уменьшать flush и compaction.
- Не ставить бинарь и не рестартовать FCGI, nginx, CampaignManager, UserBind.
- Не сравнивать абсолютный `match_total` до рестарта и после.
- Не считать всплеск code 8 в минуту рестарта регрессией.
