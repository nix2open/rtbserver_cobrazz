# Матрица доступа к хостам кластера

**Кто:** nikolay_fedorenko  
**Ключ проверки:** `~/kk1`  
**Дата пробы:** 2026-11-17  
**Источник имён:** `gateway:/etc/hosts` → `docs/hosts.mac.rtb-cluster`  
**Как проверяли:** SSH BatchMode с ноутбука — `nikolay_fedorenko@host` и `root@host`; для user — `sudo -n true`.

## Уровни

| Уровень | Значение |
|---------|----------|
| `root` | Есть SSH под `root` (ключ `kk1`) |
| `user+sudo(nopass)` | SSH под своим user + passwordless sudo |
| `user` | SSH под своим user, sudo без пароля нет |
| `none` | Нет входа ни user, ни root этим ключом |

`notes`: `denied` = ключ не принят; `timeout` = хост не ответил за ConnectTimeout (может быть down/firewall/другая сеть).

## Сводка

| Уровень | Кол-во хостов |
|---------|---------------|
| `root` | 11 |
| `user+sudo(nopass)` | 5 |
| `user` | 17 |
| `none` | 46 |
| **Всего** | **79** |

## Где доступ уже есть (удобно для работы)

| Host | IP | Уровень | Как заходить |
|------|----|---------|--------------|
| `adfe100` | 192.168.3.12 | `root` | `ssh -i ~/kk1 root@adfe100` |
| `adfe101` | 192.168.3.8 | `root` | `ssh -i ~/kk1 root@adfe101` |
| `adfe103` | 192.168.3.27 | `root` | `ssh -i ~/kk1 root@adfe103` |
| `adfe104` | 192.168.3.28 | `root` | `ssh -i ~/kk1 root@adfe104` |
| `adfe105` | 192.168.3.35 | `root` | `ssh -i ~/kk1 root@adfe105` |
| `adfe200` | 192.168.3.7 | `root` | `ssh -i ~/kk1 root@adfe200` |
| `adfe201` | 192.168.3.10 | `root` | `ssh -i ~/kk1 root@adfe201` |
| `adfe202` | 192.168.3.11 | `root` | `ssh -i ~/kk1 root@adfe202` |
| `adfe203` | 192.168.3.29 | `root` | `ssh -i ~/kk1 root@adfe203` |
| `adfe205` | 192.168.3.36 | `root` | `ssh -i ~/kk1 root@adfe205` |
| `bb` | 192.168.3.61 | `root` | `ssh -i ~/kk1 root@bb` |
| `adbe00` | 192.168.3.2 | `user+sudo(nopass)` | `ssh -i ~/kk1 nikolay_fedorenko@adbe00` (+ `sudo`) |
| `docker00` | 192.168.3.53 | `user+sudo(nopass)` | `ssh -i ~/kk1 nikolay_fedorenko@docker00` (+ `sudo`) |
| `postdb-stby00` | 192.168.3.70 | `user+sudo(nopass)` | `ssh -i ~/kk1 nikolay_fedorenko@postdb-stby00` (+ `sudo`) |
| `postdb00` | 192.168.3.4 | `user+sudo(nopass)` | `ssh -i ~/kk1 nikolay_fedorenko@postdb00` (+ `sudo`) |
| `ui00` | 192.168.3.5 | `user+sudo(nopass)` | `ssh -i ~/kk1 nikolay_fedorenko@ui00` (+ `sudo`) |
| `adbe03` | 192.168.3.19 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@adbe03` |
| `balancer03` | 192.168.3.49 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@balancer03` |
| `balancer04` | 192.168.3.90 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@balancer04` |
| `click00` | 192.168.3.63 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@click00` |
| `dev02` | 192.168.3.15 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev02` |
| `dev05-ubuntu` | 192.168.3.69 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev05-ubuntu` |
| `dev06-ubuntu` | 192.168.3.55 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev06-ubuntu` |
| `dev07-ubuntu` | 192.168.3.56 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev07-ubuntu` |
| `dev11-ubuntu` | 192.168.3.75 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev11-ubuntu` |
| `dev12-oracle` | 192.168.3.74 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@dev12-oracle` |
| `ext-gw` | 192.168.3.83 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@ext-gw` |
| `filestorage00` | 192.168.3.91 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@filestorage00` |
| `gateway` | 192.168.3.1 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@gateway` |
| `stat00` | 192.168.3.14 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@stat00` |
| `ubuntu-dev10` | 192.168.3.68 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@ubuntu-dev10` |
| `vpn00` | 192.168.3.122 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@vpn00` |
| `yuri01` | 192.168.3.64 | `user` | `ssh -i ~/kk1 nikolay_fedorenko@yuri01` |

## Куда, скорее всего, стоит просить доступ

Приоритет для RTB/релиза:

| Приоритет | Host | IP | Сейчас | Заметка |
|-----------|------|----|--------|---------|
| high | `adbe01` | 192.168.3.13 | `user:denied; root:denied` | DSP BE — есть user на adbe00/adbe03; остальные без доступа |
| high | `adbe02` | 192.168.3.3 | `user:denied; root:denied` | DSP BE — есть user на adbe00/adbe03; остальные без доступа |
| high | `adbe04` | 192.168.3.32 | `user:denied; root:denied` | DSP BE — есть user на adbe00/adbe03; остальные без доступа |
| high | `adbe05` | 192.168.3.33 | `user:denied; root:denied` | DSP BE — есть user на adbe00/adbe03; остальные без доступа |
| high | `adbe06` | 192.168.3.34 | `user:denied; root:denied` | DSP BE — есть user на adbe00/adbe03; остальные без доступа |
| high | `adfe102` | 192.168.3.9 | `user:timeout; root:timeout` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe106` | 192.168.3.38 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe107` | 192.168.3.40 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe108` | 192.168.3.44 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe109` | 192.168.3.43 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe110` | 192.168.3.52 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe204` | 192.168.3.30 | `user:timeout; root:timeout` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe206` | 192.168.3.39 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe207` | 192.168.3.42 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe208` | 192.168.3.45 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe209` | 192.168.3.51 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe210` | 192.168.3.58 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `adfe211` | 192.168.3.59 | `user:denied; root:denied` | DSP FE — сейчас root только на части FE; на остальных denied/timeout |
| high | `cms` | 192.168.3.18 | `user:denied; root:denied` | Сборка config RPM |
| medium | `balancer01` | 192.168.3.37 | `user:denied; root:denied` | Трафик/VIPs |
| medium | `balancer02` | 192.168.3.6 | `user:denied; root:denied` | Трафик/VIPs |
| medium | `click01` | 192.168.3.84 | `user:denied; root:denied` | Стриминг/аналитика |
| medium | `dev03` | 192.168.3.22 | `user:denied; root:denied` | Dev/CI окружения |
| medium | `dev13-oracle` | 192.168.3.73 | `user:timeout; root:timeout` | Dev/CI окружения |
| medium | `dev14-oracle` | 192.168.3.72 | `user:denied; root:denied` | Dev/CI окружения |
| medium | `kafka01` | 192.168.3.25 | `user:denied; root:denied` | Стриминг/аналитика |
| medium | `kafka02` | 192.168.3.41 | `user:denied; root:denied` | Стриминг/аналитика |
| medium | `kafka03` | 192.168.3.46 | `user:denied; root:denied` | Стриминг/аналитика |
| medium | `ubuntu-dev08` | 192.168.3.66 | `user:denied; root:denied` | Dev/CI окружения |
| check | `dev15-ubuntu` | 192.168.3.77 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `grafana` | 192.168.3.62 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `nvidia00` | 192.168.3.47 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `nvidia01` | 192.168.3.50 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `storage00` | 192.168.3.21 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `storage01` | 192.168.3.31 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `svn` | 192.168.3.17 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `ubuntu-dev09` | 192.168.3.67 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `ui01` | 192.168.3.78 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `windows10-yuri` | 192.168.3.71 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `yuri00` | 192.168.3.76 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `yuri01-virt` | 192.168.3.79 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `yuri02-virt` | 192.168.3.80 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| check | `yuri03-virt` | 192.168.3.81 | `user:timeout; root:timeout` | Не ответил — уточнить, жив ли хост / нужен ли VPN/маршрут |
| low | `bi00` | 192.168.3.24 | `user:denied; root:denied` | Нет ключа |
| low | `proxmox-backup` | 192.168.3.57 | `user:denied; root:denied` | Нет ключа |
| low | `pve1` | 192.168.3.60 | `user:denied; root:denied` | Нет ключа |

## Полная таблица по ролям

### DSP frontend (adfe)

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `adfe100` | 192.168.3.12 | adfe100 | no | yes | n/a | `root` | user:denied |
| `adfe101` | 192.168.3.8 | adfe101 | no | yes | n/a | `root` | user:denied |
| `adfe102` | 192.168.3.9 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `adfe103` | 192.168.3.27 | adfe103 | no | yes | n/a | `root` | user:denied |
| `adfe104` | 192.168.3.28 | adfe104 | no | yes | n/a | `root` | user:denied |
| `adfe105` | 192.168.3.35 | adfe105 | no | yes | n/a | `root` | user:denied |
| `adfe106` | 192.168.3.38 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe107` | 192.168.3.40 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe108` | 192.168.3.44 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe109` | 192.168.3.43 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe110` | 192.168.3.52 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe200` | 192.168.3.7 | adfe200 | no | yes | n/a | `root` | user:denied |
| `adfe201` | 192.168.3.10 | adfe201 | no | yes | n/a | `root` | user:denied |
| `adfe202` | 192.168.3.11 | adfe202 | no | yes | n/a | `root` | user:denied |
| `adfe203` | 192.168.3.29 | adfe203 | no | yes | n/a | `root` | user:denied |
| `adfe204` | 192.168.3.30 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `adfe205` | 192.168.3.36 | adfe205 | no | yes | n/a | `root` | user:denied |
| `adfe206` | 192.168.3.39 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe207` | 192.168.3.42 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe208` | 192.168.3.45 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe209` | 192.168.3.51 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe210` | 192.168.3.58 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adfe211` | 192.168.3.59 | — | no | no | n/a | `none` | user:denied; root:denied |

### DSP backend (adbe)

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `adbe00` | 192.168.3.2 | adbe00 | yes | no | nopass | `user+sudo(nopass)` | root:denied |
| `adbe01` | 192.168.3.13 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adbe02` | 192.168.3.3 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adbe03` | 192.168.3.19 | adbe03 | yes | no | no | `user` | root:denied |
| `adbe04` | 192.168.3.32 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adbe05` | 192.168.3.33 | — | no | no | n/a | `none` | user:denied; root:denied |
| `adbe06` | 192.168.3.34 | — | no | no | n/a | `none` | user:denied; root:denied |

### Balancer

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `balancer01` | 192.168.3.37 | — | no | no | n/a | `none` | user:denied; root:denied |
| `balancer02` | 192.168.3.6 | — | no | no | n/a | `none` | user:denied; root:denied |
| `balancer03` | 192.168.3.49 | balancer03 | yes | no | no | `user` | root:denied |
| `balancer04` | 192.168.3.90 | balancer04 | yes | no | no | `user` | root:denied |

### Postgres

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `postdb-stby00` | 192.168.3.70 | postdb-stby00 | yes | no | nopass | `user+sudo(nopass)` | root:denied |
| `postdb00` | 192.168.3.4 | postdb00 | yes | no | nopass | `user+sudo(nopass)` | root:denied |

### FOROS UI

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `ui00` | 192.168.3.5 | ui00 | yes | no | nopass | `user+sudo(nopass)` | root:denied |
| `ui01` | 192.168.3.78 | — | no | no | n/a | `none` | user:timeout; root:timeout |

### Kafka

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `kafka01` | 192.168.3.25 | — | no | no | n/a | `none` | user:denied; root:denied |
| `kafka02` | 192.168.3.41 | — | no | no | n/a | `none` | user:denied; root:denied |
| `kafka03` | 192.168.3.46 | — | no | no | n/a | `none` | user:denied; root:denied |

### ClickHouse

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `click00` | 192.168.3.63 | click00 | yes | no | no | `user` | root:denied |
| `click01` | 192.168.3.84 | — | no | no | n/a | `none` | user:denied; root:denied |

### Dev

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `dev02` | 192.168.3.15 | dev02 | yes | no | no | `user` | root:denied |
| `dev03` | 192.168.3.22 | — | no | no | n/a | `none` | user:denied; root:denied |
| `dev05-ubuntu` | 192.168.3.69 | dev05-ubuntu | yes | no | no | `user` | root:denied |
| `dev06-ubuntu` | 192.168.3.55 | dev06-ubuntu | yes | no | no | `user` | root:denied |
| `dev07-ubuntu` | 192.168.3.56 | dev07-ubuntu | yes | no | no | `user` | root:denied |
| `dev11-ubuntu` | 192.168.3.75 | ubuntu-template | yes | no | no | `user` | root:denied |
| `dev12-oracle` | 192.168.3.74 | dev12-ol | yes | no | no | `user` | root:denied |
| `dev13-oracle` | 192.168.3.73 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `dev14-oracle` | 192.168.3.72 | — | no | no | n/a | `none` | user:denied; root:denied |
| `dev15-ubuntu` | 192.168.3.77 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `ubuntu-dev08` | 192.168.3.66 | — | no | no | n/a | `none` | user:denied; root:denied |
| `ubuntu-dev09` | 192.168.3.67 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `ubuntu-dev10` | 192.168.3.68 | ubuntu-dev10 | yes | no | no | `user` | root:denied |

### Infra / management

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `cms` | 192.168.3.18 | — | no | no | n/a | `none` | user:denied; root:denied |
| `ext-gw` | 192.168.3.83 | ext-gw | yes | no | no | `user` | root:denied |
| `gateway` | 192.168.3.1 | newgateway | yes | no | no | `user` | root:denied |
| `svn` | 192.168.3.17 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `vpn00` | 192.168.3.122 | vpn01 | yes | no | no | `user` | root:denied |

### Infra / apps

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `bb` | 192.168.3.61 | monitoring | no | yes | n/a | `root` | user:denied |
| `bi00` | 192.168.3.24 | — | no | no | n/a | `none` | user:denied; root:denied |
| `docker00` | 192.168.3.53 | docker00 | yes | no | nopass | `user+sudo(nopass)` | root:denied |
| `grafana` | 192.168.3.62 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `proxmox-backup` | 192.168.3.57 | — | no | no | n/a | `none` | user:denied; root:denied |
| `pve1` | 192.168.3.60 | — | no | no | n/a | `none` | user:denied; root:denied |
| `stat00` | 192.168.3.14 | stat00 | yes | no | no | `user` | root:denied |

### Storage

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `filestorage00` | 192.168.3.91 | filestorage00 | yes | no | no | `user` | root:denied |
| `storage00` | 192.168.3.21 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `storage01` | 192.168.3.31 | — | no | no | n/a | `none` | user:timeout; root:timeout |

### Other / personal

| Host | IP | Hostname | User SSH | Root SSH | sudo -n | Уровень | Notes |
|------|----|----------|----------|----------|---------|---------|-------|
| `nvidia00` | 192.168.3.47 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `nvidia01` | 192.168.3.50 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `windows10-yuri` | 192.168.3.71 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `yuri00` | 192.168.3.76 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `yuri01` | 192.168.3.64 | yuri01 | yes | no | no | `user` | root:denied |
| `yuri01-virt` | 192.168.3.79 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `yuri02-virt` | 192.168.3.80 | — | no | no | n/a | `none` | user:timeout; root:timeout |
| `yuri03-virt` | 192.168.3.81 | — | no | no | n/a | `none` | user:timeout; root:timeout |

## Что попросить (черновик для админов)

1. **adfe106–110, adfe206–211** (+ проверить `adfe102`/`adfe204` timeout): SSH `root` тем же ключом `kk1`, либо user `nikolay_fedorenko` + sudo — для canary/раскатки FE.
2. **adbe01,02,04,05,06**: user `nikolay_fedorenko` + sudo (как на `adbe00`) — для backend-релиза.
3. **cms**: user/SSH или иной способ собирать config RPM.
4. **dev02**: passwordless sudo (или набор build-deps) — чтобы собирать `foros-server` без Juri.
5. Уточнить статус хостов с `timeout` (down / только ILO / другой VRF).

## Как повторить пробу

```bash
# см. логику в сессии; краткий one-host check:
ssh -i ~/kk1 -o BatchMode=yes -o ConnectTimeout=5 nikolay_fedorenko@HOST true
ssh -i ~/kk1 -o BatchMode=yes -o ConnectTimeout=5 root@HOST true
```

Сырые данные пробы: можно пересобрать из gateway hosts + SSH.
