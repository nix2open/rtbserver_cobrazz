# RTB Cobrazz — сборка и деплой релиза

Инструкция для разработчиков и ИИ-агентов. После прочтения можно выполнить релиз **без дополнительных уточнений**, используя команды ниже.

**Последнее обновление:** 2026-11-26
**Платформа:** Cobrazz DSP (`foros-server-central` + config colo `programmatic`)
**Сборка:** `dev02`
**Исходники:** GitLab `http://ubuntu-dev08` (группа `genius-desk`)

---

## 0. Роли хостов (кратко)

| Роль | Хосты | Назначение |
|------|-------|------------|
| Build | `dev02` (`192.168.3.15`) | `RPM/build.sh`, `cfgen buildRpm`; источник RPM |
| Hub / cmgr | `adbe00` | `/tmp/REL_*` для fan-out; `/opt/foros/manager/bin/cmgr` |
| DSP Frontend FE1 | `adfe100–110` (без `adfe102` в части конфигов) | bid / HttpFrontend / FCGI* |
| DSP Frontend FE2 | `adfe200–211` | то же, второй пул |
| DSP Backend | `adbe00–06` | CampaignServer, RIM, EM, … |

Colo name в cmgr: **`programmatic`**.
На FE1 сервисы: `tr1-*` / `fe1-*`. На FE2: `tr2-*` / `fe2-*`.

**Поток артефактов (внутри кластера):** `dev02` (сборка) → `adbe00` (hub) → `adfe*`/`adbe*`.
Ноутбук — только SSH-оркестрация (`ssh`/`cmgr`); **байты RPM через ноутбук не гонять**.

### 0.1. Трафик на FE (важно для рестарта)

На каждом `adfe` пути разведены по **разным** процессам. Рестарт одного не обязан ронять остальные.

| Трафик | Примеры путей | cmgr-сервис (FE1 / FE2) | Потеря при рестарте |
|--------|---------------|-------------------------|---------------------|
| Bid / OpenRTB | `/openrtb`, `/bid`, … | `tr*-FCGIRtbServer` | допустима |
| Показы (imp) | `/track.gif`, `/services/ImprTrack/pt.gif` | `tr*-FCGITrackServer` | **нежелательна** |
| Pixel / conv | `/conv`, `/pubpixels`, `/passback.gif`, `/sl.gif` | `tr*-FCGIActionServer` | **нежелательна** |
| Edge (все пути) | любой HTTP | `tr*-HttpFrontend` (nginx) | рестарт бьёт **всё** на хосте |
| verify / action gRPC | — | `fe*-CampaignManager` | рестарт рвёт imp/pixel даже при живом Track/Action |

Правило: рестартовать **только** сервисы, затронутые релизом. `/openrtb` можно терять; `/imp` и pixel — нет.

---

## 1. Доступы и ключи (обязательные факты)

### 1.1. SSH с ноутбука разработчика

Ключ: **`~/kk1`** (абсолютный путь: `/Users/n.fedorenko/kk1`).

Ноутбук нужен **только как точка входа** (запуск команд / `cmgr` / ForwardAgent).
Между `dev02`/`adbe00`/`adfe*` своих ключей обычно нет — для **межхостового** `scp`
внутри кластера: `ssh-add ~/kk1` и `ssh -A ...` (agent). На hop **не** ставить
`IdentitiesOnly=yes`, иначе agent key не подхватится. Данные RPM при этом идут
хост→хост, не через Mac.

```bash
ssh-add ~/kk1   # один раз за сессию (для -A)

# build / publish hub
ssh -A -i ~/kk1 -o IdentitiesOnly=yes -o ForwardAgent=yes nikolay_fedorenko@dev02

# canary / install (root на части adfe) — точечные проверки с ноутбука OK
ssh -i ~/kk1 -o IdentitiesOnly=yes root@adfe101

# hub / cmgr / fan-out
ssh -A -i ~/kk1 -o IdentitiesOnly=yes -o ForwardAgent=yes nikolay_fedorenko@adbe00
```

`/etc/hosts` на Mac должен резолвить имена кластера (эталон: `gateway:/etc/hosts`, снимок в `docs/hosts.mac.rtb-cluster`). Если имени нет — используй IP из §0 / gateway.

### 1.2. Права (проверено)

| Хост | Аккаунт | Права |
|------|---------|-------|
| `dev02` | `nikolay_fedorenko` | **NOPASSWD sudo ALL** + GitLab через `~/.netrc` (group deploy token `dev02-build`) |
| `adbe00` | `nikolay_fedorenko` | **NOPASSWD sudo ALL**; `cmgr` работает |
| `adfe101`, `adfe103`, `adfe100`, `adfe104`, `adfe105`, `adfe200`, `adfe201`, `adfe202`, `adfe203`, `adfe205` | `root` via `kk1` | Можно `yum install` |
| Остальные adfe (напр. `adfe106–110`, `adfe206–211`) | — | Часто **нет** root/`kk1` — не выбирать их для canary без проверки |

Перед canary **всегда** проверить:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes -o BatchMode=yes -o ConnectTimeout=5 root@adfe101 'hostname; rpm -q foros-server-central'
```

### 1.3. GitLab

| Repo | URL |
|------|-----|
| server | `http://ubuntu-dev08/genius-desk/rtbserver_cobrazz.git` |
| unixcommons | `http://ubuntu-dev08/genius-desk/unixcommons_cobrazz.git` |
| configs | `http://ubuntu-dev08/genius-desk/configs_cobrazz.git` |

Web UI: `http://ubuntu-dev08/genius-desk/<name>`
Прод-линия кода: ветка **`modern`** (есть на GitLab). Default branch в UI может быть `main` — для релиза checkout **`modern`**.

На `dev02` у `nikolay_fedorenko` clone по HTTP **без ручного логина** (deploy token в `~/.netrc`).
Не коммитить токены в git.

---

## 2. Артефакты релиза

Нужны **5 RPM** для binary+config (или только config-тройку, если меняется только конфиг):

1. `foros-server-central-4.0.0.19-ssvNNN.el8.x86_64.rpm`
2. `foros-telegraf-1.38.1-ssv1.el8.x86_64.rpm` — класть в тот же `REL/` (§5.2.1)
3. `foros-config-server-programmatic-4.0.0.19-cms*.noarch.rpm`
4. `foros-config-server-programmatic-mgr-4.0.0.19-cms*.noarch.rpm`
5. `foros-config-server-programmatic-local-mgr-4.0.0.19-cms*.noarch.rpm`

`Version` пакета сейчас: **`4.0.0.19`**.
`Release` бинарника: **`ssvNNN`** в `RPM/SPECS/server.spec` (`Release: ssvNNN%{?dist}`).
Config release: timestamp `cmsYYYYMMDDHHMMSS` от `cfgen`.

Опционально debuginfo/debugsource — для обычного upgrade **не обязательны**.

---

## 3. Подготовка кода

1. Внеси изменения в нужную ветку (`modern` или feature → merge в `modern`).
2. Запушь в GitLab `genius-desk/rtbserver_cobrazz` (и при необходимости `unixcommons_cobrazz` / `configs_cobrazz`).
3. Для **бинарного** релиза подними `Release` в `RPM/SPECS/server.spec` (например `ssv653` → `ssv680`) и закоммить. Сообщение коммита принято вида `ssv680`.

**Config-only релиз:** меняй `configs_cobrazz` (`adserver.xml`) и/или `CMS/Plugin` в rtbserver; бинарь можно не пересобирать, если код не менялся.

---

## 4. Сборка на `dev02` (пошагово)

Выполнять **на `dev02`** под `nikolay_fedorenko`. Для §4.4 (publish на `adbe00`) —
сессия с ForwardAgent:

```bash
ssh-add ~/kk1
ssh -A -i ~/kk1 -o IdentitiesOnly=yes -o ForwardAgent=yes nikolay_fedorenko@dev02
```

### 4.1. Чистый workspace

```bash
RELEASE=ssv680          # ← поставить актуальный номер Release
VERSION=4.0.0.19
WORKDIR=$HOME/build-rtb-$RELEASE
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

git clone http://ubuntu-dev08/genius-desk/unixcommons_cobrazz.git
git clone http://ubuntu-dev08/genius-desk/rtbserver_cobrazz.git
git clone http://ubuntu-dev08/genius-desk/configs_cobrazz.git

cd rtbserver_cobrazz
git checkout modern
git pull --ff-only origin modern
# убедись, что в RPM/SPECS/server.spec Release: $RELEASE
grep '^Release:' RPM/SPECS/server.spec
```

### 4.2. Бинарь

```bash
cd "$WORKDIR/rtbserver_cobrazz"
sudo ./RPM/build.sh "$VERSION" "$(pwd)/../unixcommons_cobrazz/"
```

Результат (копия):

```bash
ls -1 "$WORKDIR/rtbserver_cobrazz/build/RPMS"/foros-server-central-*.rpm
```

Возьми **без** `debuginfo`/`debugsource`, если не нужны:

```bash
mkdir -p "$WORKDIR/REL"
cp -v "$WORKDIR/rtbserver_cobrazz/build/RPMS"/foros-server-central-${VERSION}-${RELEASE}.el8.x86_64.rpm "$WORKDIR/REL/" || \
cp -v "$WORKDIR/rtbserver_cobrazz/build/RPMS"/foros-server-central-*.el8.x86_64.rpm "$WORKDIR/REL/"
# foros-telegraf — обязателен в REL для yum --disablerepo='*' (Requires в server.spec)
cp -v "$WORKDIR/rtbserver_cobrazz/build/RPMS"/foros-telegraf-*.rpm "$WORKDIR/REL/" 2>/dev/null || \
  cp -v /root/rpmbuild/RPMS/x86_64/foros-telegraf-*.rpm "$WORKDIR/REL/" 2>/dev/null || \
  echo "WARN: foros-telegraf RPM not found — найди его или поставка yum на FE упадёт"
# удали debug* из REL, если скопировались
rm -f "$WORKDIR/REL"/*debug*.rpm
```

Сборка долгая (десятки минут). Не прерывать SSH без `tmux`/`screen` — рекомендуется:

```bash
tmux new -s rtbbuild
# ... build ...
```

**telegraf на этапе `build.sh`:** `RPM/build-telegraf.sh` качает upstream RPM с
`http://192.168.10.1/...`. Зеркало часто **недоступно / зависает на 0%**. Если файл уже
есть в `$(rpm -E %_sourcedir)/telegraf-1.38.1-1.x86_64.rpm` (типично после прошлых
сборок) — **не перекачивать**, переиспользовать локальный и проверить sha256. Можно
патчить workspace-копию `build-telegraf.sh` только на время сборки (reuse-if-exists);
в git это не коммитить без отдельного решения.

### 4.3. Config RPM

```bash
cd "$WORKDIR"
mkdir -p "$WORKDIR/REL"
/opt/cms/bin/cfgen -o "$WORKDIR/REL/" buildRpm \
  "$WORKDIR/configs_cobrazz/adserver.xml" \
  "$WORKDIR/rtbserver_cobrazz/CMS/Plugin/"
ls -1 "$WORKDIR/REL"/foros-config-server-programmatic*.rpm
```

Должны появиться ровно три config RPM (+ бинарь, если собирали).

### 4.4. Плоский каталог и публикация на hub (`adbe00`)

`cfgen` кладёт config RPM во вложенный путь (`REL/rpms/.../AdServer-.../`). Собери
**плоский** каталог на `dev02`, затем **сразу** залей его на `adbe00` по внутренней
сети (не скачивать на ноутбук).

```bash
# на dev02, после §4.2–4.3
FLAT="$WORKDIR/REL_FLAT"
mkdir -p "$FLAT"
cp -v "$WORKDIR/REL"/foros-server-central-${VERSION}-${RELEASE}.el8.x86_64.rpm "$FLAT/"
cp -v "$WORKDIR/REL"/foros-telegraf-*.rpm "$FLAT/" 2>/dev/null || \
  cp -v "$WORKDIR/rtbserver_cobrazz/build/RPMS"/foros-telegraf-*.rpm "$FLAT/" 2>/dev/null || \
  echo "WARN: нет foros-telegraf — возьми из прошлого build-rtb-*/build/RPMS"
find "$WORKDIR/REL" -name 'foros-config-server-programmatic*.rpm' -exec cp -v {} "$FLAT/" \;
rm -f "$FLAT"/*debug*.rpm
ls -lh "$FLAT"
# ожидаемо 5 RPM: central + telegraf + 3 config

# publish → hub (нужен ForwardAgent с ноутбука: ssh -A ...@dev02)
ssh -o BatchMode=yes -o StrictHostKeyChecking=no nikolay_fedorenko@adbe00 \
  "mkdir -p /tmp/REL_${RELEASE}"
scp -o BatchMode=yes -o StrictHostKeyChecking=no "$FLAT"/*.rpm \
  nikolay_fedorenko@adbe00:/tmp/REL_${RELEASE}/
# проверка размера/sha central на hub vs FLAT
ssh -o BatchMode=yes -o StrictHostKeyChecking=no nikolay_fedorenko@adbe00 \
  "ls -lh /tmp/REL_${RELEASE}; sha256sum /tmp/REL_${RELEASE}/foros-server-central-*.rpm"
```

Дальше (§5) все хосты берут пакеты **только с `adbe00:/tmp/REL_${RELEASE}/`**.

---

## 5. Деплой (canary → остальные)

### 5.1. Выбор canary

По умолчанию: **`adfe103`** (рядовой FE1, есть root). Альтернативы с root: `adfe101`, `adfe105`, `adfe200`, `adfe205`.

Не брать единственный контроллерный хост без нужды для правок, завязанных только на `UserBindController` / `ChannelController` (часто `adfe101`/`adfe100`).

### 5.2. Установить на canary (пакеты уже на hub)

Hub уже заполнен в §4.4 (`adbe00:/tmp/REL_${RELEASE}/`). На canary и ферму копировать
**только с `adbe00`**. Не `scp` с ноутбука на `adfe`/`adbe` (медленно, truncated RPM).

```bash
RELEASE=ssvNNN
CANARY=adfe103
ssh-add ~/kk1
ssh -A -i ~/kk1 -o IdentitiesOnly=yes -o ForwardAgent=yes nikolay_fedorenko@adbe00 bash -s <<EOF
set -euo pipefail
SRC=/tmp/REL_${RELEASE}
# на hop с adbe00 НЕ ставить IdentitiesOnly=yes — иначе agent key не подхватится
ssh -o BatchMode=yes -o StrictHostKeyChecking=no root@${CANARY} "mkdir -p /root/REL_${RELEASE}"
scp -o BatchMode=yes -o StrictHostKeyChecking=no \$SRC/*.rpm root@${CANARY}:/root/REL_${RELEASE}/
ssh -o BatchMode=yes -o StrictHostKeyChecking=no root@${CANARY} \
  "cd /root/REL_${RELEASE} && yum install -y --disablerepo='*' --allowerasing ./*.rpm"
EOF
```

Установка на сам `adbe00` (user + sudo), если нужен backend — локально с hub-каталога:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00 \
  "cd /tmp/REL_${RELEASE} && sudo yum install -y --disablerepo='*' --allowerasing ./*.rpm"
```

Проверка версии:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  'rpm -q foros-server-central foros-config-server-programmatic foros-telegraf'
```

#### 5.2.1. Зависимости `foros-telegraf` и `python3.12-*` (частый блокер)

`foros-server-central` в `RPM/SPECS/server.spec` требует в том числе:

- `foros-telegraf >= 1.38.1`
- `python3.12` и пакеты `python3.12-requests`, `python3.12-aiohttp`, …

При установке **`yum install --disablerepo='*'`** yum **не** ходит во внешние репозитории
и удовлетворяет Requires **только** из RPM, лежащих в текущем каталоге (и уже
установленных на хосте).

Практика на FE/BE (проверено на ssv686):

| Пакет | Типичное состояние на FE | Что делать |
|-------|--------------------------|------------|
| `foros-telegraf` | обычно уже стоит `1.38.1-ssv1` | **всё равно** клади `foros-telegraf-*.rpm` в `REL/` — иначе yum с `--disablerepo='*'` часто падает с `nothing provides foros-telegraf` |
| `python3.12-requests` | часто **не установлен**, при этом текущий central уже может иметь Unsatisfied dependencies на него | зеркала yum с FE (`ol8_*`, `influxdb`, …) нередко **таймаутятся** — `yum install python3.12-requests` с включёнными repo не опираться |

**Fallback (как уже делают на ферме при битой dep-цепочке python):** не включать
внешние repo ради одного Requires; поставить бинарь через `rpm` с `--nodeps`, конфиги —
обычным `rpm -Uvh`:

```bash
cd /root/REL_${RELEASE}   # на adbe00: /tmp/REL_${RELEASE} + sudo
rpm -Uvh --nodeps foros-server-central-${VERSION}-${RELEASE}.el8.x86_64.rpm
rpm -Uvh --force foros-telegraf-1.38.1-ssv1.el8.x86_64.rpm   # no-op, если уже та же версия
rpm -Uvh foros-config-server-programmatic-*.rpm \
         foros-config-server-programmatic-mgr-*.rpm \
         foros-config-server-programmatic-local-mgr-*.rpm
rpm -q foros-server-central foros-config-server-programmatic foros-telegraf
```

`--nodeps` здесь **только** из‑за известного пробела `python3.12-requests` на хостах
(модуль для рантайма DSP не обязателен для bid/CM/Billing). Не использовать `--nodeps`
«на всякий случай» для других недостающих библиотек — сначала разобрать ошибку.

`yum install` **без** `--disablerepo='*'` на FE не предпочитать: мёртвые mirror’ы
(`192.168.15.1`, `repos.influxdata.com`) дают долгий hang / fail metadata.

### 5.3. Что рестартовать (матрица)

Перед `cmgr restart` определи тип изменений. **Не** рестартовать «на всякий случай» nginx / CM / Track / Action.

| Что изменилось | Рестартовать | Не трогать |
|----------------|--------------|------------|
| Только bidding / OpenRTB | `tr*-FCGIRtbServer` | `HttpFrontend`, `FCGITrackServer`, `FCGIActionServer`, `CampaignManager` (если API CM не менялся) |
| Только ImprTrack / click / adinst | `tr*-FCGITrackServer` | Rtb, Action; nginx — если конфиг nginx не менялся |
| Только pixel / conv / passback | `tr*-FCGIActionServer` | остальное |
| nginx / locations / FeConfig edge | `tr*-HttpFrontend` (+ нужные FCGI) | — |
| Логика `verify_impression` / CM API | `fe*-CampaignManager` (см. порядок ниже) | не вместе с nginx без нужды |
| Config-only без смены nginx upstream | только FCGI, которые читают новый FeConfig | nginx, если `nginx.conf` не менялся |
| Несовместимые изменения внутренних протоколов (LP / gRPC / бинарные логи между BE↔FE) | полный порядок §5.7 (zero-downtime) | обычный canary-only restart из §5.4 |

**Порядок на одном хосте:**

1. `yum install` (старые процессы ещё работают со старым бинарём в RAM).
2. `cmgr restart` **только** нужных сервисов из матрицы.
3. Дождаться `running` + smoke (§5.4).
4. Если нужен рестарт **CampaignManager**: сначала убедиться, что Track/Action здоровы после любых их рестартов; CM рестартовать **отдельно**, не в одной команде с nginx. Пока CM down — in-flight imp/pixel на этом хосте теряются.
5. `HttpFrontend` — только если менялся nginx/config.

### 5.4. Рестарт сервисов canary

С **`adbe00`** (cmgr). FE2: префиксы `tr2` / `fe2`.

**Default для типичного bid/binary релиза** (не трогаем imp/pixel edge):

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00
H=adfe103
/opt/foros/manager/bin/cmgr -f programmatic restart ${H}:tr1-FCGIRtbServer
/opt/foros/manager/bin/cmgr -f programmatic -x status ${H}:tr1-FCGIRtbServer
```

**Если менялись nginx + bidding + CM** (полный набор; бьёт все пути на хосте):

```bash
H=adfe103
/opt/foros/manager/bin/cmgr -f programmatic restart \
  ${H}:tr1-FCGIRtbServer ${H}:fe1-CampaignManager ${H}:tr1-HttpFrontend
/opt/foros/manager/bin/cmgr -f programmatic -x status \
  ${H}:tr1-FCGIRtbServer ${H}:fe1-CampaignManager ${H}:tr1-HttpFrontend
```

Ожидание: `running`.
Если `cmgr restart` завис/не поднял процесс — fallback **только на зависший сервис** (не на nginx + все FCGI сразу):

```bash
# пример: завис только Rtb
/opt/foros/manager/bin/cmgr -f programmatic stop ${H}:tr1-FCGIRtbServer
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${H} 'pkill -9 -f FCGIRtbServer || true'
/opt/foros/manager/bin/cmgr -f programmatic start ${H}:tr1-FCGIRtbServer
```

Широкий `pkill -9 FCGI; pkill -9 nginx` — **запрещён** как первый шаг: роняет Track/Action и все показы на хосте.

### 5.5. Мониторинг canary (минимум 5–15 минут)

Подставь в `status` те же сервисы, что рестартовал.

```bash
# status (пример bid-only)
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00 \
  "/opt/foros/manager/bin/cmgr -f programmatic -x status ${CANARY}:tr1-FCGIRtbServer"

# nginx / edge жив (даже если nginx не рестартовали)
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} 'curl -sS --max-time 2 http://127.0.0.1:10180/status | head'

# ошибки: рестартованные сервисы + Track/Action/CM (imp/pixel не должны «посыпаться»)
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  'tail -n 50 /opt/foros/server/var/log/FCGIRtbServer*/FCGIRtbServer*.error 2>/dev/null | tail -50; \
   tail -n 30 /opt/foros/server/var/log/FCGITrackServer*/FCGITrackServer*.error 2>/dev/null | tail -30; \
   tail -n 30 /opt/foros/server/var/log/FCGIActionServer*/FCGIActionServer*.error 2>/dev/null | tail -30; \
   tail -n 30 /opt/foros/server/var/log/CampaignManager/CampaignManager.error 2>/dev/null | tail -30'
```

Сравни с соседним FE того же пула. При аномалиях (рост ошибок Track/Action/CM, падение status) — **не раскатывать дальше**; откати пакеты предыдущей версии с canary (`yum downgrade` / установка старых RPM из предыдущего `COBRAZZ*` каталога, если сохранились).

### 5.6. Раскат на остальные хосты

Только после успешного canary.

Установка RPM может идти пачкой (процессы ещё на старом коде в RAM). **Рестарт — строго по одному хосту** (или ≤2), с wait `running` + короткий smoke между хостами. Не гонять весь FE1/FE2 for-loop рестартов без паузы.

**Копирование пакетов — только с hub `adbe00`** (см. §4.4 / §5.2). Пример для root-FE:

```bash
RELEASE=ssvNNN   # тот же, что на hub
ssh-add ~/kk1
ssh -A -i ~/kk1 -o IdentitiesOnly=yes -o ForwardAgent=yes nikolay_fedorenko@adbe00 bash -s <<EOF
set -euo pipefail
SRC=/tmp/REL_${RELEASE}
SSH_OPTS="-o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=12"
# IdentitiesOnly на hop НЕ ставить — иначе ForwardAgent не сработает
for h in adfe100 adfe101 adfe103 adfe104 adfe105 adfe200 adfe201 adfe202 adfe203 adfe205; do
  echo "==== \$h ===="
  ssh \$SSH_OPTS root@\$h "mkdir -p /root/REL_${RELEASE}" || { echo "ssh_fail \$h"; continue; }
  scp \$SSH_OPTS \$SRC/*.rpm root@\$h:/root/REL_${RELEASE}/ || { echo "scp_fail \$h"; continue; }
  ssh \$SSH_OPTS root@\$h \
    "cd /root/REL_${RELEASE} && yum install -y --disablerepo='*' --allowerasing ./*.rpm"
done
EOF
```

Хосты с `nikolay_fedorenko` + passwordless sudo (`adbe01+`, часть FE вроде `adfe106–110` /
`adfe206–211`): тот же fan-out с `adbe00`, dest `/tmp/REL_${RELEASE}/`, установка через
`sudo yum` / `sudo rpm` (§5.2.1).

Рестарт с `adbe00` (пример **bid-only**; для полного набора — см. матрицу §5.3):

```bash
# FE1 — по одному хосту
for h in adfe100 adfe101 adfe103 adfe104 adfe105; do
  echo "==== restart $h ===="
  /opt/foros/manager/bin/cmgr -f programmatic restart $h:tr1-FCGIRtbServer
  /opt/foros/manager/bin/cmgr -f programmatic -x status $h:tr1-FCGIRtbServer
  # дождаться running, затем 1–2 мин smoke / сверка логов, потом следующий хост
done
# FE2
for h in adfe200 adfe201 adfe202 adfe203 adfe205; do
  echo "==== restart $h ===="
  /opt/foros/manager/bin/cmgr -f programmatic restart $h:tr2-FCGIRtbServer
  /opt/foros/manager/bin/cmgr -f programmatic -x status $h:tr2-FCGIRtbServer
done
/opt/foros/manager/bin/cmgr -f programmatic status | grep -E 'stopp|fail|FCGIRtb|FCGITrack|FCGIAction|HttpFrontend' | head -50
```

Хосты **без** SSH-доступа — пропустить или отдать тому, у кого есть ключ / через репозиторий на gateway (Ваня).

**Backend (`adbe*`):** ставить отдельно, если релиз затрагивает backend. На `adbe00` —
hub + `sudo yum install ...`. На `adbe01+` — копировать **с `adbe00`**, ставить через
`nikolay_fedorenko` + `sudo` (проверить доступ перед раскатом).

Альтернатива доставки RPM: репозиторий на **gateway00** (уточнять у Вани). Не тащить
RPM на ноутбук и не раздавать с него.

### 5.7. Zero-downtime при несовместимых внутренних протоколах

Использовать **вместо** обычного «restart только FCGIRtbServer» (§5.4/§5.6), когда релиз ломает совместимость внутренних протоколов между backend и frontend (LP/log processing, gRPC, бинарные логи и т.п.). Иначе новые FE будут говорить со старыми BE (или наоборот) и поймают ошибки протокола.

Обычный canary (§5.1–5.5) всё равно желателен: сначала поставить пакеты и прогнать smoke на одном FE, **если** canary может работать со смешанными версиями достаточно для проверки. Если смешанный режим принципиально нежизнеспособен — идти сразу по порядку ниже на всю ферму, но только после явного решения человека.

Все команды `cmgr` — с **`adbe00`**, colo `programmatic`. Суффикс `-` у маски = **исключение** (не рестартовать).

#### Порядок (FE1 → PreStart → FE2 → PreStart)

**Шаг A — пакеты на `adbe*` и FE1 (`adfe1*`), затем рестарты FE1**

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00
# 1) yum install новых RPM на adbe* и adfe1* (доставка — как в §5.2/§5.6; на adbe00 через sudo)
# 2) сначала log-processing (lp-*) на backend и FE1
/opt/foros/manager/bin/cmgr -f programmatic restart 'adbe*:lp-*' 'adfe1*:lp-*'

# 3) остальные сервисы на FE1, кроме lp-*, FCGI*, Http*
/opt/foros/manager/bin/cmgr -f programmatic restart \
  'adfe1*' 'adfe1*:lp-*-' '*:*FCGI*-' '*:*Http*-'

# 4) FCGI + HttpFrontend — по одному хосту (меньше всплеск реконнектов на SSP)
#    По факту их можно было рестартовать на шаге 3, но per-host безопаснее.
for h in adfe{100..111}; do
  echo "==== FE1 edge $h ===="
  /opt/foros/manager/bin/cmgr -f programmatic restart "$h:*FCGI*" "$h:*Http*"
  /opt/foros/manager/bin/cmgr -f programmatic -x status "$h:*FCGI*" "$h:*Http*"
  # дождаться running; короткий smoke, затем следующий хост
done
```

**Шаг B — дождаться PreStart (не прерывать, пока cmgr не вернёт управление)**

```bash
/opt/foros/manager/bin/cmgr -f programmatic start '*:*-PreStart'
# ждём без Ctrl-C: команда должна завершиться сама — это сигнал, что загрузка прошла
```

**Шаг C — пакеты на FE2 (`adfe2*`), затем рестарты FE2**

```bash
# 5) yum install на adfe2* (доставка как в §5.6)
# 6) сервисы FE2 кроме FCGI/Http
/opt/foros/manager/bin/cmgr -f programmatic restart \
  'adfe2*' '*:*FCGI*-' '*:*Http*-'

# 7) FCGI + Http — по одному хосту
for h in adfe{200..211}; do
  echo "==== FE2 edge $h ===="
  /opt/foros/manager/bin/cmgr -f programmatic restart "$h:*FCGI*" "$h:*Http*"
  /opt/foros/manager/bin/cmgr -f programmatic -x status "$h:*FCGI*" "$h:*Http*"
done
```

**Шаг D — снова PreStart**

```bash
/opt/foros/manager/bin/cmgr -f programmatic start '*:*-PreStart'
# снова ждать возврата управления — убедиться, что всё загрузилось
```

#### Замечания к §5.7

- Маски `adfe1*` / `adfe2*` — FE1 (`adfe100–111`) и FE2 (`adfe200–211`). Хосты без доступа / отсутствующие в colo cmgr просто пропустит или вернёт ошибку по ним — сверить с §1.2 и `docs/CLUSTER_ACCESS_MATRIX.md`.
- Per-host рестарт `*FCGI*` / `*Http*` на шагах 4 и 7 — чтобы не устроить одновременный reconnect storm на стороне SSP.
- `*:*-PreStart` (`tr*-PreStart` / `fe*-PreStart`) — проверка готовности зависимостей; **не убивать** сессию, пока команда не завершится.
- Этот путь шире обычного bid-only: он трогает `HttpFrontend`, Track/Action и пр. Ожидай кратковременную потерю edge-трафика **на каждом хосте по очереди**, не на всём пуле сразу.
- После завершения — смоук как в §5.5 на нескольких FE1 и FE2 + `cmgr status` по ферме.

---

## 6. Чеклист для ИИ-агента (исполнять по порядку)

Скопируй и отмечай:

1. [ ] Определи тип релиза: `binary+config` / `config-only`.
2. [ ] Есть ли **несовместимые** изменения внутренних протоколов? Если да → путь §5.7; иначе §5.3/§5.4.
3. [ ] По §5.3 составь список сервисов для restart (default bid-only → только `FCGIRtbServer`).
4. [ ] Выставь `RELEASE=ssvNNN`, `VERSION=4.0.0.19`, `CANARY=adfe103`.
5. [ ] Проверь SSH: `dev02` (user+sudo), `CANARY` (root), `adbe00` (user+cmgr).
6. [ ] На `dev02`: clone 3 репо с GitLab, `git checkout modern` в rtbserver.
7. [ ] Для binary: проверь/подними `Release` в `RPM/SPECS/server.spec`, `sudo ./RPM/build.sh ...`.
8. [ ] `cfgen -o REL/ buildRpm configs_cobrazz/adserver.xml rtbserver_cobrazz/CMS/Plugin/`.
9. [ ] На `dev02` собери **плоский** `REL_FLAT/`: central + **foros-telegraf** + 3 config (без debug*). §4.4.
10. [ ] С `dev02` (ForwardAgent) опубликуй `REL_FLAT/` → hub `adbe00:/tmp/REL_${RELEASE}/`; сверь sha. RPM **не** качать на ноутбук.
11. [ ] С hub → CANARY + `yum install --disablerepo='*' --allowerasing` (если canary применим). При `nothing provides foros-telegraf` / `python3.12-requests` → §5.2.1 (`rpm -Uvh --nodeps` для central).
12. [ ] Совместимый релиз: `cmgr restart` **только** выбранных сервисов CANARY; дождись `running`. Не рестартуй nginx/CM/Track/Action без нужды (§0.1, §5.3).
13. [ ] Несовместимый протокол: после решения человека — порядок §5.7 (lp → FE1 без edge → FE1 edge per-host → PreStart → FE2 → PreStart).
14. [ ] Смоук: `rpm -q`, `cmgr status`, `curl :10180/status`, error-логи Rtb **и** Track/Action/CM.
15. [ ] Если OK (совместимый путь) — fan-out RPM **с `adbe00`** + yum на остальные adfe; restart **по одному хосту** с wait + smoke; иначе STOP и откат canary.
16. [ ] Итог: список хостов с новой версией `rpm -q foros-server-central foros-config-server-programmatic`.

**Запрещено агенту без явного запроса человека:**
- `yum install` сразу на всю ферму без canary;
- качать RPM на ноутбук или `scp` с ноутбука на `adfe`/`adbe` (поток: `dev02` → `adbe00` → хосты, §0 / §4.4);
- рестарт `HttpFrontend` / `CampaignManager` / `FCGITrack*` / `FCGIAction*` «на всякий случай» при bid-only релизе;
- массовый `cmgr restart` всех FE без паузы между хостами (для edge FCGI/Http — только per-host, см. §5.7);
- полный порядок §5.7 без подтверждения, что релиз реально ломает внутренние протоколы;
- `pkill -9` nginx / всех FCGI как первый шаг;
- force-push / удаление веток;
- коммит секретов (токены, `.netrc`, ключи).

---

## 7. Типичные проблемы

| Симптом | Что делать |
|---------|------------|
| `git clone` на dev02 просит логин | Проверить `~/.netrc` и доступность `http://ubuntu-dev08`; не коммитить новый токен в git |
| `build.sh` падает на deps | `sudo yum install` недостающих `-devel` по тексту ошибки; deps обычно уже на dev02 |
| `build-telegraf.sh` / curl telegraf висит 0% | Зеркало `192.168.10.1` недоступно; reuse `%_sourcedir/telegraf-1.38.1-1.x86_64.rpm` (§4.2) |
| Нет `build/RPMS` | Смотри вывод `rpmbuild`; также `rpm -E '%_rpmdir/%_arch'` |
| yum: `nothing provides foros-telegraf` | Положи `foros-telegraf-*.rpm` в тот же каталог, что central (§5.2.1) |
| yum: `nothing provides python3.12-requests` | На FE пакет часто отсутствует; внешние repo таймаутятся → `rpm -Uvh --nodeps` для central (§5.2.1) |
| `yum list` / metadata hang на FE | Мёртвые repo (`influxdb`, `192.168.15.1`); не снимай `--disablerepo='*'` ради deps |
| `cmgr` → Host key verification / fail | Запускать cmgr с **adbe00**; маски FE1=`tr1/fe1`, FE2=`tr2/fe2` |
| `Permission denied` root@adfeXXX | Хост не в зоне ключа `kk1` — другой canary или запросить доступ |
| Config не применился | Убедиться, что установлены **все три** config RPM и сделан restart Http/FCGI |
| Ошибки протокола / LP после частичного раската | Вероятно несовместимый внутренний протокол при смешанных версиях — довести порядок §5.7 или откатить |
| `cmgr start *:*-PreStart` долго не возвращает управление | Не прерывать; смотреть логи PreStart на FE; дождаться завершения или явной ошибки |

---

## 8. Связанные документы в репо

- `docs/hosts.mac.rtb-cluster` — hosts для Mac
- `docs/CLUSTER_ACCESS_MATRIX.md` — матрица доступов (снимок)
- `docs/RTB_RELEASE_GUIDE_FOR_JURI.md` — черновик согласования с Jurij

Этот файл (`docs/RTB_RELEASE.md`) — **основной runbook** для сборки и выкладки.
