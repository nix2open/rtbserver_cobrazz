# RTB Cobrazz — сборка и деплой релиза

Инструкция для разработчиков и ИИ-агентов. После прочтения можно выполнить релиз **без дополнительных уточнений**, используя команды ниже.

**Последнее обновление:** 2026-11-19  
**Платформа:** Cobrazz DSP (`foros-server-central` + config colo `programmatic`)  
**Сборка:** `dev02`  
**Исходники:** GitLab `http://ubuntu-dev08` (группа `genius-desk`)

---

## 0. Роли хостов (кратко)

| Роль | Хосты | Назначение |
|------|-------|------------|
| Build | `dev02` (`192.168.3.15`) | `RPM/build.sh`, `cfgen buildRpm` |
| Management / cmgr | `adbe00` | `/opt/foros/manager/bin/cmgr` restart/status |
| DSP Frontend FE1 | `adfe100–110` (без `adfe102` в части конфигов) | bid / HttpFrontend / FCGI* |
| DSP Frontend FE2 | `adfe200–211` | то же, второй пул |
| DSP Backend | `adbe00–06` | CampaignServer, RIM, EM, … |

Colo name в cmgr: **`programmatic`**.  
На FE1 сервисы: `tr1-*` / `fe1-*`. На FE2: `tr2-*` / `fe2-*`.

---

## 1. Доступы и ключи (обязательные факты)

### 1.1. SSH с ноутбука разработчика

Ключ: **`~/kk1`** (абсолютный путь: `/Users/n.fedorenko/kk1`).

```bash
# build
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@dev02

# canary / install (root на части adfe)
ssh -i ~/kk1 -o IdentitiesOnly=yes root@adfe101

# cmgr
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00
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

Нужны **4 RPM** (или только config-тройку, если меняется только конфиг):

1. `foros-server-central-4.0.0.19-ssvNNN.el8.x86_64.rpm`  
2. `foros-config-server-programmatic-4.0.0.19-cms*.noarch.rpm`  
3. `foros-config-server-programmatic-mgr-4.0.0.19-cms*.noarch.rpm`  
4. `foros-config-server-programmatic-local-mgr-4.0.0.19-cms*.noarch.rpm`  

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

Выполнять **на `dev02`** под `nikolay_fedorenko`.

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@dev02
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
# удали debug* из REL, если скопировались
rm -f "$WORKDIR/REL"/*debug*.rpm
```

Сборка долгая (десятки минут). Не прерывать SSH без `tmux`/`screen` — рекомендуется:

```bash
tmux new -s rtbbuild
# ... build ...
```

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

### 4.4. Каталог для раскатки

```bash
ls -la "$WORKDIR/REL"
# ожидаемо:
# foros-server-central-...ssvNNN....rpm          (если binary release)
# foros-config-server-programmatic-...cms....rpm
# foros-config-server-programmatic-mgr-...
# foros-config-server-programmatic-local-mgr-...
```

---

## 5. Деплой (canary → остальные)

### 5.1. Выбор canary

По умолчанию: **`adfe103`** (рядовой FE1, есть root). Альтернативы с root: `adfe101`, `adfe105`, `adfe200`, `adfe205`.

Не брать единственный контроллерный хост без нужды для правок, завязанных только на `UserBindController` / `ChannelController` (часто `adfe101`/`adfe100`).

### 5.2. Залить RPM и установить на canary

С ноутбука (или с `dev02`, если есть ключ на adfe):

```bash
CANARY=adfe103
RELDIR=...   # путь к REL на машине, откуда scp

scp -i ~/kk1 -o IdentitiesOnly=yes "$RELDIR"/*.rpm root@${CANARY}:/root/REL_${RELEASE}/
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  "mkdir -p /root/REL_${RELEASE} && cd /root/REL_${RELEASE} && yum install -y --disablerepo='*' --allowerasing ./*.rpm"
```

Если каталог ещё не создан до scp:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} "mkdir -p /root/REL_${RELEASE}"
scp -i ~/kk1 -o IdentitiesOnly=yes "$RELDIR"/*.rpm root@${CANARY}:/root/REL_${RELEASE}/
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  "cd /root/REL_${RELEASE} && yum install -y --disablerepo='*' --allowerasing ./*.rpm"
```

Проверка версии:

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  'rpm -q foros-server-central foros-config-server-programmatic'
```

### 5.3. Рестарт сервисов canary

С **`adbe00`** (cmgr):

**FE1** (пример `adfe103`):

```bash
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00
H=adfe103
/opt/foros/manager/bin/cmgr -f programmatic restart \
  ${H}:tr1-HttpFrontend ${H}:tr1-FCGIRtbServer ${H}:fe1-CampaignManager
/opt/foros/manager/bin/cmgr -f programmatic -x status \
  ${H}:tr1-HttpFrontend ${H}:tr1-FCGIRtbServer ${H}:fe1-CampaignManager
```

**FE2** (пример `adfe200`): префиксы `tr2` / `fe2`.

Ожидание: `running`.  
Если `cmgr restart` завис/не поднял процесс — fallback (как в старых выкладках):

```bash
/opt/foros/manager/bin/cmgr -f programmatic stop ${H}:tr1-HttpFrontend ${H}:tr1-FCGI* ${H}:fe1-CampaignManager
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${H} 'pkill -9 FCGI; pkill -9 nginx; pkill -9 CampaignM || true'
/opt/foros/manager/bin/cmgr -f programmatic start ${H}:tr1-HttpFrontend ${H}:tr1-FCGI* ${H}:fe1-CampaignManager
```

### 5.4. Мониторинг canary (минимум 5–15 минут)

```bash
# status
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00 \
  "/opt/foros/manager/bin/cmgr -f programmatic -x status ${CANARY}:tr1-HttpFrontend ${CANARY}:tr1-FCGIRtbServer ${CANARY}:fe1-CampaignManager"

# nginx status (типичные порты 10180–10183)
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} 'curl -sS --max-time 2 http://127.0.0.1:10180/status | head'

# ошибки в логах (пример)
ssh -i ~/kk1 -o IdentitiesOnly=yes root@${CANARY} \
  'tail -n 50 /opt/foros/server/var/log/CampaignManager/CampaignManager.error 2>/dev/null; tail -n 50 /opt/foros/server/var/log/FCGIRtbServer*/FCGIRtbServer*.error 2>/dev/null | tail -50'
```

Сравни с соседним FE того же пула. При аномалиях — **не раскатывать дальше**; откати пакеты предыдущей версии с canary (`yum downgrade` / установка старых RPM из предыдущего `COBRAZZ*` каталога, если сохранились).

### 5.5. Раскат на остальные хосты

Только после успешного canary.

Для каждого хоста с доступом root:

```bash
for h in adfe100 adfe101 adfe103 adfe104 adfe105 adfe200 adfe201 adfe202 adfe203 adfe205; do
  echo "==== $h ===="
  ssh -i ~/kk1 -o IdentitiesOnly=yes -o BatchMode=yes -o ConnectTimeout=8 root@$h \
    "mkdir -p /root/REL_${RELEASE}" || continue
  scp -i ~/kk1 -o IdentitiesOnly=yes "$RELDIR"/*.rpm root@$h:/root/REL_${RELEASE}/
  ssh -i ~/kk1 -o IdentitiesOnly=yes root@$h \
    "cd /root/REL_${RELEASE} && yum install -y --disablerepo='*' --allowerasing ./*.rpm"
done
```

Рестарт пачками с `adbe00`:

```bash
# FE1
for h in adfe100 adfe101 adfe103 adfe104 adfe105; do
  /opt/foros/manager/bin/cmgr -f programmatic restart \
    $h:tr1-HttpFrontend $h:tr1-FCGIRtbServer $h:fe1-CampaignManager
done
# FE2
for h in adfe200 adfe201 adfe202 adfe203 adfe205; do
  /opt/foros/manager/bin/cmgr -f programmatic restart \
    $h:tr2-HttpFrontend $h:tr2-FCGIRtbServer $h:fe2-CampaignManager
done
/opt/foros/manager/bin/cmgr -f programmatic status | grep -E 'stopp|fail|HttpFrontend|FCGIRtb' | head -50
```

Хосты **без** SSH-доступа — пропустить или отдать тому, у кого есть ключ / через репозиторий на gateway (Ваня).

**Backend (`adbe*`):** ставить отдельно, если релиз затрагивает backend. На `adbe00` ставить через `nikolay_fedorenko` + `sudo yum install ...`. На `adbe01+` доступ может отсутствовать — проверить перед раскатом.

Альтернатива доставки RPM: репозиторий на **gateway00** (уточнять у Вани) вместо `scp` файлов.

---

## 6. Чеклист для ИИ-агента (исполнять по порядку)

Скопируй и отмечай:

1. [ ] Определи тип релиза: `binary+config` / `config-only`.
2. [ ] Выставь `RELEASE=ssvNNN`, `VERSION=4.0.0.19`, `CANARY=adfe103`.
3. [ ] Проверь SSH: `dev02` (user+sudo), `CANARY` (root), `adbe00` (user+cmgr).
4. [ ] На `dev02`: clone 3 репо с GitLab, `git checkout modern` в rtbserver.
5. [ ] Для binary: проверь/подними `Release` в `RPM/SPECS/server.spec`, `sudo ./RPM/build.sh ...`.
6. [ ] `cfgen -o REL/ buildRpm configs_cobrazz/adserver.xml rtbserver_cobrazz/CMS/Plugin/`.
7. [ ] Собери каталог `REL/` из нужных RPM (без debug*, если не нужны).
8. [ ] `scp` + `yum install --disablerepo='*' --allowerasing` **только на CANARY**.
9. [ ] `cmgr restart` сервисов CANARY; дождись `running`.
10. [ ] Смоук: `rpm -q`, `cmgr status`, `curl :10180/status`, хвост error-логов.
11. [ ] Если OK — раскат на остальные доступные adfe + restart; иначе STOP и откат canary.
12. [ ] Итог: список хостов с новой версией `rpm -q foros-server-central foros-config-server-programmatic`.

**Запрещено агенту без явного запроса человека:**
- `yum install` сразу на всю ферму без canary;
- `pkill -9` на всех хостах как первый шаг;
- force-push / удаление веток;
- коммит секретов (токены, `.netrc`, ключи).

---

## 7. Типичные проблемы

| Симптом | Что делать |
|---------|------------|
| `git clone` на dev02 просит логин | Проверить `~/.netrc` и доступность `http://ubuntu-dev08`; не коммитить новый токен в git |
| `build.sh` падает на deps | `sudo yum install` недостающих `-devel` по тексту ошибки; deps обычно уже на dev02 |
| Нет `build/RPMS` | Смотри вывод `rpmbuild`; также `rpm -E '%_rpmdir/%_arch'` |
| `cmgr` → Host key verification / fail | Запускать cmgr с **adbe00**; маски FE1=`tr1/fe1`, FE2=`tr2/fe2` |
| `Permission denied` root@adfeXXX | Хост не в зоне ключа `kk1` — другой canary или запросить доступ |
| Config не применился | Убедиться, что установлены **все три** config RPM и сделан restart Http/FCGI |

---

## 8. Связанные документы в репо

- `docs/hosts.mac.rtb-cluster` — hosts для Mac  
- `docs/CLUSTER_ACCESS_MATRIX.md` — матрица доступов (снимок)  
- `docs/RTB_RELEASE_GUIDE_FOR_JURI.md` — черновик согласования с Jurij  

Этот файл (`docs/RTB_RELEASE.md`) — **основной runbook** для сборки и выкладки.
