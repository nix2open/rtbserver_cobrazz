# RTB Cobrazz: сборка и релиз (актуальная схема от Jurij)

**Дата обновления:** 2026-11-18  
**Источник:** инструкция Jurij + проверка на `dev02`  
**Сборка:** `dev02` (`192.168.3.15`)  
**Статус:** процедура рестарта — ждём от Jurij; canary на один хост — желательно сохранить.

---

## 0. Что нужно для upgrade

| RPM | Откуда |
|-----|--------|
| `foros-server-central-*.rpm` | `sudo ./RPM/build.sh …` |
| `foros-config-server-programmatic-*.rpm` | `/opt/cms/bin/cfgen buildRpm …` |
| `foros-config-server-programmatic-mgr-*.rpm` | то же |
| `foros-config-server-programmatic-local-mgr-*.rpm` | то же |

Эти пакеты ставятся на целевые хосты (все нужные adfe/adbe), затем рестарт сервисов.

---

## 1. Сборка на dev02

### 1.1. Доступ

```bash
ssh -i ~/kk1 nikolay_fedorenko@dev02
```

Проверено 2026-11-18 на `dev02`:

- есть `git`, `rpmbuild`, `cmake3`, `gcc-toolset-10`
- build-deps для текущего `server.spec` поставлены (folly / boost185 / rocksdb / grpc)
- есть `/opt/cms/bin/cfgen`
- `/var` снова со свободным местом
- **sudo всё ещё с паролем** (`sudo -n` не проходит) — для `build.sh` нужен интерактивный sudo или nopass

### 1.2. Бинарь `foros-server-central`

Инструкция Jurij (с правкой имени репо — см. §4):

```bash
mkdir -p ~/K && cd ~/K

# предпочтительно GitLab (зеркало yoori, группа genius-desk):
git clone http://ubuntu-dev08/genius-desk/unixcommons_cobrazz.git
git clone http://ubuntu-dev08/genius-desk/rtbserver_cobrazz.git
# альтернатива (upstream): https://github.com/yoori/...

cd rtbserver_cobrazz
# при необходимости: git checkout modern   # или нужный commit/tag ssvNNN
# на GitLab-зеркале проверьте наличие ветки (см. docs / GitLab UI)

sudo ./RPM/build.sh 4.0.0.19 "$(pwd)/../unixcommons_cobrazz/"
```

Ожидаемый выход: RPM `foros-server-central` (обычно под `build/RPMS/` / rpmbuild tree).

`Release` (`ssvNNN`) задаётся в `RPM/SPECS/server.spec` — перед релизом поднять номер.

### 1.3. Config RPM (programmatic)

```bash
cd ~/K
git clone http://ubuntu-dev08/genius-desk/configs_cobrazz.git

# plugin берём из дерева rtbserver (CMS/Plugin)
/opt/cms/bin/cfgen buildRpm \
  configs_cobrazz/adserver.xml \
  rtbserver_cobrazz/CMS/Plugin/
```

Ожидаемый выход (тройка):

- `foros-config-server-programmatic`
- `foros-config-server-programmatic-mgr`
- `foros-config-server-programmatic-local-mgr`

> В сообщении Jurij путь к plugin был  
> `/home/jurij_kuznecov/projects/rtbserver_targetrtb/CMS/Plugin/` —  
> на практике для Cobrazz это `…/rtbserver_cobrazz/CMS/Plugin/` (см. §4).

Полезные опции `cfgen`: `-o <output-dir>`, `-help`.

---

## 2. Установка на хосты

По Jurij: RPM нужны для upgrade; ставить **на все нужные хосты**.

Два варианта:

1. **Через репозиторий на `gateway00`** — детали у Вани  
2. **Файлами** (как сейчас в истории деплоев):

```bash
# на каждом хосте, из каталога с rpm:
sudo yum install -y --disablerepo='*' --allowerasing ./*.rpm
```

Рекомендуемый безопасный порядок (пока не согласовано иное):

1. один canary-хост (например `adfe101` / `adfe103`)
2. мониторинг
3. остальные FE1 → FE2 → при необходимости adbe

Доступ для установки: `root` **или** user + passwordless `sudo ALL` (достаточно любого из двух).

---

## 3. Рестарт

Jurij: *«Процедуру рестарта скину чуть позже».*

Пока ориентир (из практики colo):

```bash
# с management-хоста (часто adbe00), пример FE1:
/opt/foros/manager/bin/cmgr -f programmatic restart \
  adfe101:tr1-HttpFrontend \
  adfe101:tr1-FCGIRtbServer \
  adfe101:fe1-CampaignManager

/opt/foros/manager/bin/cmgr -f programmatic -x status \
  adfe101:tr1-HttpFrontend \
  adfe101:tr1-FCGIRtbServer \
  adfe101:fe1-CampaignManager
```

FE2: префиксы `tr2` / `fe2`.  
Точную маску сервисов после upgrade — по инструкции Jurij, когда пришлёт.

---

## 4. Замечания / уточнить у Jurij

1. В тексте было `git clone …/rtbserver_targetrtb.git` + `pushd rtbserver_cobrazz` — репо `rtbserver_targetrtb` на GitHub **404**. Для Cobrazz клонируем **`rtbserver_cobrazz`**.
2. Plugin path с `rtbserver_targetrtb` — то же; для programmatic colo ожидаем plugin из `rtbserver_cobrazz`.
3. Нужен ли перед сборкой явный `git checkout modern` / тег `ssvNNN`?
4. Куда `cfgen` кладёт RPM по умолчанию (`-o`?) и нужно ли класть debuginfo в upgrade.
5. `gateway00` vs `gateway` (`192.168.3.1` / `newgateway`) — это один хост или отдельный repo-host у Вани?
6. Ждём процедуру рестарта.

---

## 5. Короткий чеклист релиза

- [ ] На `dev02`: clone unixcommons + rtbserver (+ checkout нужной ревизии)
- [ ] `sudo ./RPM/build.sh 4.0.0.19 ../unixcommons_cobrazz/`
- [ ] clone `configs_cobrazz`
- [ ] `cfgen buildRpm adserver.xml …/CMS/Plugin/`
- [ ] Сложить 4 RPM в каталог релиза
- [ ] Canary install на 1 хост → status/мониторинг
- [ ] Раскат на остальные хосты (или через gateway00 repo)
- [ ] Рестарт по процедуре Jurij
- [ ] Проверка `cmgr status` / логи / трафик

---

## 6. GitLab (ubuntu-dev08) — зеркала yoori

Созданы в группе `genius-desk` (private), залиты с `github.com/yoori/...` (все ветки/теги на момент зеркала):

| Repo | URL |
|------|-----|
| rtbserver | http://ubuntu-dev08/genius-desk/rtbserver_cobrazz |
| unixcommons | http://ubuntu-dev08/genius-desk/unixcommons_cobrazz |
| configs | http://ubuntu-dev08/genius-desk/configs_cobrazz |

Дополнительно в `rtbserver_cobrazz` запушена локальная ветка **`modern`** (`ssv679`) — на GitHub `yoori` её уже нет (404).

Clone для сборки на `dev02` (нужен доступ к private GitLab):

```bash
git clone http://ubuntu-dev08/genius-desk/unixcommons_cobrazz.git
git clone http://ubuntu-dev08/genius-desk/rtbserver_cobrazz.git
git clone http://ubuntu-dev08/genius-desk/configs_cobrazz.git
cd rtbserver_cobrazz && git checkout modern
```

На `dev02` для `nikolay_fedorenko` уже прописан **group deploy token** (`genius-desk` / `read_repository`) в `~/.netrc` — обычный `git clone http://ubuntu-dev08/...` работает без ручного логина.  
Отозвать: GitLab → `genius-desk` → Settings → Repository → Deploy tokens (`dev02-build`).

## 7. Связанные файлы

- Матрица доступов: `docs/CLUSTER_ACCESS_MATRIX.md`
- Hosts для Mac: `docs/hosts.mac.rtb-cluster`
