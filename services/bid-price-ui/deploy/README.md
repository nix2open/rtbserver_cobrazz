# Bid price UI

Спека: `docs/BID_PRICE_SPEC.md` §8. Порт **3091**.

## Хост

Канонически — `adbe05:3091`. На момент фазы 2 `adbe05` **недоступен по SSH**
(`docs/CLUSTER_ACCESS_MATRIX.md`: user/root denied). Поэтому BFF поднят на
**`adbe00:3091`** (тот же VPN/кластер, ClickHouse `click00` рядом). Когда появится
доступ к `adbe05` — перенести unit и `/opt/bid-price-ui` туда.

## Деплой (adbe00)

```bash
# с ноутбука
tar czf /tmp/bid-price-ui.tgz -C services/bid-price-ui backend deploy
scp -i ~/kk1 -o IdentitiesOnly=yes /tmp/bid-price-ui.tgz nikolay_fedorenko@adbe00:/tmp/
ssh -i ~/kk1 -o IdentitiesOnly=yes nikolay_fedorenko@adbe00
sudo mkdir -p /opt/bid-price-ui && sudo tar xzf /tmp/bid-price-ui.tgz -C /opt/bid-price-ui
sudo python3.12 -m venv /opt/bid-price-ui/venv
sudo /opt/bid-price-ui/venv/bin/pip install -r /opt/bid-price-ui/backend/requirements.txt
sudo cp /opt/bid-price-ui/deploy/config.env.example /opt/bid-price-ui/config.env
sudo cp /opt/bid-price-ui/deploy/bid-price-ui.service /etc/systemd/system/
# ExecStart должен указывать на venv uvicorn
sudo systemctl daemon-reload && sudo systemctl enable --now bid-price-ui
curl -sS http://127.0.0.1:3091/api/health
```

Туннель с ноутбука: `ssh -i ~/kk1 -o IdentitiesOnly=yes -L 3091:127.0.0.1:3091 nikolay_fedorenko@adbe00`
