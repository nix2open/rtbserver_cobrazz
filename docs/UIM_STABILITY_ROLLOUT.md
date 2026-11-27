# UIM: стабильность match и fcap

Инструкция для агента. Выполнять сверху вниз, когда человек просит раскатить.
Сборка и доставка RPM — по `docs/RTB_RELEASE.md`. Этот файл задаёт только состав
изменения, проверки и порядок рестарта.

Цель: меньше отказов `match` с gRPC code 8 (`MaxWaitersReached`) и короче match,
при живой отдаче профиля и fcap. Данные пользователя не отключать.

Первый релиз — **config-only**. Бинарь `foros-server-central` не пересобирать.
Код (этап 5) — отдельное решение после выкладки конфига, не в том же окне.

## Что меняем

Оба `UserInfoManager` в `configs_cobrazz/adserver.xml` (FrontendSubCluster-1 и -2),
узел `configuration:matchParams`:

| Атрибут | Сейчас на хостах | Ставим |
|---------|------------------|--------|
| `rocksdb_batching_threads` | 4 | `8` |
| `compaction_threads` | 32 | `8` |
| `flush_threads` | 32 | `4` |
| `per_compaction_threads` | 4 | `4` |
| `max_mem_tables` | 6 | `6` |
| `rocksdb_cache_size` | 0 | `4294967296` (4 ГиБ, байты) |

`max_base_profile_waiters` в `adserver.xml` не задаётся. Его пишет
`CMS/Plugin/xslt/UserInfoProfiling/UserInfoManager.xsl`: заменить `8` на `32`
только у `max_base_profile_waiters`. `max_temp_profile_waiters` и
`max_freqcap_profile_waiters` оставить `8`.

То же значение `matchParams` продублировать в
`CMS/tests/Configs/cobrazz/adserver.xml`, если этот файл ещё сверяют с colo.

Не менять: `ret_freq_caps`, `silent_match`, `clean_user_profiles`, число чанков,
UserBind, FCGI, nginx, CampaignManager.

## Хосты и имена

| | |
|--|--|
| Canary | `adfe103` |
| Контроль, пока его не трогали | `adfe105` |
| cmgr | с `adbe00`, colo `programmatic` |
| Сервис FE1 | `fe1-UserInfoManager` |
| Сервис FE2 | `fe2-UserInfoManager` |
| Конфиг на хосте | `/opt/foros/server/etc/programmatic/adcluster/<host>/UserInfoManagerConfig.xml` |
| Статистика | `http://127.0.0.1:10701/stats` |
| gRPC | `<host>:10601` |
| SSH | `ssh -i ~/kk1 -o IdentitiesOnly=yes` |

FE1: `adfe100 adfe101 adfe103 adfe104 adfe105 adfe106 adfe107 adfe108 adfe109 adfe110`.
FE2: `adfe200 adfe201 adfe202 adfe203 adfe205 adfe206 adfe207 adfe208 adfe209 adfe210 adfe211`.
`adfe102` и `adfe204` в этом сервисе нет.

Рестарт одного UIM на несколько минут убирает его чанки из раздачи (у canary это
3 из 32, около 167 ГБ). Открытие базы может занять несколько минут. Процесс не
убивать. `pkill` — не шаг этого плана.

Полный `cmgr status` без `-x` не запускать. Если имя сервиса не находится,
остановиться и уточнить его, не перебирать маски по всей ферме.

## Метрики

Счётчики `/stats` копятся с старта процесса. Сравнивать скорости за окно, не
абсолютные числа и не процесс до рестарта с процессом после.

Окно базовой линии и окно canary — **15 минут** каждое, после прогрева.
Прогрев после рестарта — **5 минут** с момента, когда `match_total` уже растёт.
Минуту рестарта в окно не включать.

Снять два снимка `/stats` с паузой 60 секунд и посчитать:

| Имя | Формула |
|-----|---------|
| `match_rps` | Δ`match_total` / Δt |
| `match_avg_us` | Δ`match_total_time` / Δ`match_total` |
| `saves_per_match` | Δ`rdb_save_total` / Δ`match_total` |
| `gets_per_match` | Δ`rdb_get_total` / Δ`match_total` |
| `fcap_update_rps` | Δ`update_user_freq_caps_total` / Δt |
| `fcap_confirm_rps` | Δ`confirm_user_freq_caps_total` / Δt |
| `cache_hit_ratio` | Δ`rdb_cache_hits` / (Δ`rdb_cache_hits` + Δ`rdb_cache_misses`) |

Плюс один снимок: `call_in_progress`, `match_in_progress`, RSS, `%CPU`,
`MemAvailable`, load average, `rdb_cache_limit`.

Отказы code 8 считать на **вызывающих** FCGI, не в `UserInfoManager.error`.
Брать доступные фронты `adfe100`, `adfe103`, `adfe200`. В
`/opt/foros/server/var/log/FCGIRtbServer/FCGIRtbServer.error` за те же 15 минут
считать строки `UserInfoManager grpc match failed: code=8` с
`grpc_endpoint=<host>:10601`. Отдельно для canary и для контроля.

`match_total_time` — микросекунды.

Команда снимка (на целевом хосте):

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@adfe103 'bash -s' << 'EOF'
python3 - << 'PY'
import json, time, urllib.request
def snap():
    with urllib.request.urlopen("http://127.0.0.1:10701/stats", timeout=3) as r:
        return json.load(r), time.time()
a, t0 = snap()
time.sleep(60)
b, t1 = snap()
dt = t1 - t0
def d(k):
    return b.get(k, 0) - a.get(k, 0)
match = d("match_total")
print(f"dt_s={dt:.1f}")
print(f"match_rps={match/dt:.1f}")
print(f"match_avg_us={(d('match_total_time')/match) if match else 0:.0f}")
print(f"saves_per_match={(d('rdb_save_total')/match) if match else 0:.3f}")
print(f"gets_per_match={(d('rdb_get_total')/match) if match else 0:.3f}")
print(f"fcap_update_rps={d('update_user_freq_caps_total')/dt:.2f}")
print(f"fcap_confirm_rps={d('confirm_user_freq_caps_total')/dt:.2f}")
hits, misses = d("rdb_cache_hits"), d("rdb_cache_misses")
print(f"cache_hit_ratio={(hits/(hits+misses)) if hits+misses else 0:.3f}")
print(f"cache_limit={b.get('rdb_cache_limit')}")
print(f"match_in_progress={b.get('match_in_progress')}")
print(f"call_in_progress={b.get('call_in_progress')}")
PY
ps -C UserInfoManager -o pcpu=,rss=,etime=
awk '/MemAvailable/{printf "mem_available_mb=%d\n",$2/1024}' /proc/meminfo
uptime
EOF
```

Подсчёт code 8 за 15 минут на одном фронте (подставить endpoint):

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@adfe100 'python3 -' << 'PY'
import os, time
p="/opt/foros/server/var/log/FCGIRtbServer/FCGIRtbServer.error"
needle=b"grpc_endpoint=adfe103:10601"
cut=time.time()-15*60
# файл текущий; если ротация была внутри окна, сказать об этом и не усреднять дыру
st=os.stat(p)
print(f"size={st.st_size} age_s={time.time()-st.st_mtime:.0f}")
n=0
# читаем хвост до 40 МБ: этого хватает на 15 минут при текущем темпе
with open(p,"rb") as f:
    f.seek(max(0, st.st_size-40_000_000))
    for line in f:
        if b"code=8" in line and needle in line and b"grpc match failed" in line:
            n+=1
print(f"code8={n} per_min={n/15:.1f}")
PY
```

Ту же команду с `adfe105:10601` снять в том же интервале. Суммировать три фронта.

Записать таблицу до и после: canary, контроль, сумма code 8.

## Этап 0. Базовая линия

Ничего не ставить и не рестартовать.

1. На `adfe103` и `adfe105` снять метрики командами выше.
2. Code 8 по endpoint canary и контроля — сумма с трёх фронтов.
3. Сохранить цифры в ответ человеку. Без этой записи этап 3 не начинать.
4. Убедиться, что UIM на canary `running`, `match_rps` > 0, `fcap_update_rps` > 0.

Ориентир с 2026-11-26, не норма: `match_avg_us` около 22000, `saves_per_match` около 1,
`gets_per_match` около 2, `rdb_cache_limit=0`, code 8 идёт постоянно.

## Этап 1. Сборка конфига

1. Внести правки из раздела «Что меняем». Коммит и push — только если человек попросил.
2. На `dev02` в workspace релиза должны лежать **эти** файлы, а не чистый `origin/modern`.
   Если `docs/RTB_RELEASE.md` клонирует заново, после clone перенести туда дифф.
3. Собрать только config RPM: `docs/RTB_RELEASE.md` §4.3. `RPM/build.sh` не запускать.
4. В плоский каталог для yum положить три config RPM и уже стоящий `foros-telegraf`
   (§4.4). Новый `foros-server-central` не класть.
5. До публикации сгенерированный XML canary (из дерева cfgen или уже с хоста после
   установки, до рестарта) должен содержать:

```text
rocksdb_batching_threads="8"
compaction_threads="8"
flush_threads="4"
rocksdb_cache_size="4294967296"
max_base_profile_waiters="32"
max_freqcap_profile_waiters="8"
```

Если любого пункта нет — на хосты не ставить.

## Этап 2. Canary

Только `adfe103`. Пакет ставить по §5.2 `docs/RTB_RELEASE.md`.

Рестарт с `adbe00`, один сервис:

```bash
H=adfe103
/opt/foros/manager/bin/cmgr -f programmatic restart ${H}:fe1-UserInfoManager
/opt/foros/manager/bin/cmgr -f programmatic -x status ${H}:fe1-UserInfoManager
```

Не рестартовать `FCGI*`, `HttpFrontend`, `CampaignManager`, `UserBindServer`,
`ChannelServer`, контроллер UIM.

Ждать до 10 минут:

- `cmgr` показывает `running`;
- в логе UIM есть `service started` новее старта процесса
  (`/opt/foros/server/var/log/UserInfoManager/`);
- `match_total` на `:10701/stats` увеличивается;
- `rdb_cache_limit` равен `4294967296`.

Строку `Chunk maps loaded` не ждать: она пишется на TRACE и при `log_level=7` в лог не попадает.

Дальше 5 минут прогрева и 15 минут окна. В том же интервале снять контроль `adfe105`.

## Этап 3. Решение по canary

Смотреть canary против своей базовой линии и против контроля того же окна.
Контроль нужен, чтобы общий всплеск трафика не принять за эффект рестарта.

Продолжать раскат, если одновременно:

| Проверка | Порог |
|----------|--------|
| процесс | `running`, `match_rps` от 70% до 130% своей базы |
| match | `match_avg_us` ≤ max(1.5 × база, база + 5000) и < 50000 |
| code 8 на `adfe103:10601` | ≤ 50% своей базы |
| code 8 на `adfe105:10601` | от 50% до 150% своей базы (контроль не развалился) |
| fcap | `fcap_update_rps` > 0 и от 50% до 150% базы |
| запись профиля | `saves_per_match` от 0.8 до 1.2 |
| кэш | `cache_hit_ratio` > 0.3 |
| память | RSS < база + 10 ГБ, `MemAvailable` > 20 ГБ |
| очередь | `match_in_progress` < 200 в момент снимка |
| ошибки UIM | в `.error` нет новых классов, кроме уже известного `Campaign Server is not ready` |
| соседи | error-логи Track, Action и CampaignManager на canary без нового всплеска |

Откатить canary целиком (старые три config RPM, рестарт только `fe1-UserInfoManager`), если любое из:

- за 10 минут нет `running` или `match_total` не растёт;
- `match_rps` < 70% базы дольше 5 минут после прогрева;
- `match_avg_us` ≥ 50000;
- code 8 на canary вырос относительно своей базы;
- `fcap_update_rps` упал до 0 при живом `match_rps`;
- `MemAvailable` < 15 ГБ или в логе есть нехватка места / OOM / ошибка RocksDB;
- load average на 10 минут выше базы больше чем на 20.

После отката снять те же метрики и убедиться, что `rdb_cache_limit` снова 0 и
`max_base_profile_waiters` в конфиге снова 8. Дальше не идти.

`Campaign Server is not ready` раз в несколько минут сам по себе откатом не является.

## Этап 4. Остальная ферма

Только после явного «canary годится» в цифрах этапа 3.

Пакеты можно разложить на хосты пачкой. Рестарт UIM — по одному хосту.
Между хостами: `running`, `match_total` растёт, 2 минуты без новых строк в
`UserInfoManager.error`. Полное 15-минутное окно на каждом хосте не снимать.

Порядок: остальные FE1 (`adfe100`, `adfe101`, `adfe104`, `adfe105`, затем
`adfe106`–`adfe110`), потом FE2 (`fe2-UserInfoManager`, начиная с `adfe200`).
Хост, до которого SSH нет, пропустить и написать об этом. Не считать ферму
готовой, пока такие хосты не названы.

После всего FE1 и после всего FE2 ещё раз снять 15-минутное окно code 8
суммарно по `adfe100` + `adfe103` + `adfe200` без фильтра endpoint и `match_avg_us`
на двух хостах каждого пула (`adfe103`, `adfe105`, `adfe200`, `adfe201`).

Ферма принимается, если:

- code 8 за 15 минут ≤ 50% суммы базовой линии этих трёх фронтов;
- `match_avg_us` на четырёх хостах < 50000 и не хуже своей базы больше чем в 1.5 раза;
- `fcap_update_rps` > 0 на каждом проверенном хосте;
- `saves_per_match` на каждом от 0.8 до 1.2;
- `cache_hit_ratio` > 0.3;
- ни один UIM не отсутствует в `cmgr`.

Откат хоста, который провалил короткий smoke — только этот хост, старым config RPM
и рестартом его UIM. Уже принятые хосты не откатывать пачкой, пока человек не скажет.

## Этап 5. Код, только если ферма этапа 4 не сошлась

Не начинать в том же релизе, что конфиг. Отдельное решение человека.

Делать, если через 2 часа после последнего рестарта code 8 всё ещё выше 50% базы
или `match_avg_us` на части хостов стабильно выше 40000.

Порядок работ, каждая следующим canary и тем же набором метрик:

1. Single-flight: параллельные `match` одного uid ждут один результат и все получают
   профиль и fcap. Лимит 8 на это больше не отвечает отказам.
2. Не писать базовый профиль, если каналы те же и `last_request` моложе нескольких
   секунд. Чтение профиля и fcap, а также `update_user_freq_caps` /
   `confirm_user_freq_caps`, остаются. На этом шаге `saves_per_match` имеет право
   упасть; порог 0.8–1.2 для него снять и следить, что `match_rps` и fcap живы.
3. Поднять блочный кэш RocksDB с 1 ГБ в `ProfilingCommons/ProfileMap/RocksDBOptions.cpp`.
   Это уже бинарь: сборка по `docs/RTB_RELEASE.md` §4.2, рестарт снова только UIM.

Перед бинарём прогнать узкие тесты, которые есть рядом с правкой
(`UserInfoManagerCorePerfTest`, тесты `TransactionMap` / кэша профиля). На ферму
не ставить, если они не собрались.

На canary бинаря пороги этапа 3 те же, кроме `saves_per_match` после пункта 2.
Дополнительно: в ответе match у обычного uid по-прежнему есть fcap
(`full_freq_caps` не пустой там, где до правки был не пустой). Это смотреть по
debug-bid или по счётчику `update_user_freq_caps`, а не по отключению match.

## Чего не делать

- Не выключать `ret_freq_caps` и не ставить `silent_match` на все биды.
- Не поднимать `rocksdb_batching_threads` сразу до 16 и не возвращать compaction/flush на 32 «для скорости».
- Не рестартовать UserBind, Channel, FCGI, nginx и CampaignManager из-за этого изменения.
- Не ставить RPM сразу на все хосты с рестартом в одном цикле.
- Не сравнивать абсолютные `match_total` процесса до рестарта и после.
- Не считать всплеск code 8 в минуту рестарта регрессией.
