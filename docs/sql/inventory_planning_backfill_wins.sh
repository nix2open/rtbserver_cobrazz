#!/usr/bin/env bash
# Backfill inventory_planning_daily from RImpression_recent (wins path).
# Usage: ./inventory_planning_backfill_wins.sh [days=45]
set -euo pipefail

DAYS="${1:-45}"
SSH=(ssh -i "${HOME}/kk1" -o IdentitiesOnly=yes nikolay_fedorenko@click00)

echo "Backfilling last ${DAYS} days into inventory_planning_daily..."

for i in $(seq 1 "${DAYS}"); do
  DAY=$("${SSH[@]}" "clickhouse-client --query \"SELECT toString(today() - ${i})\"")
  echo "== ${DAY} =="
  "${SSH[@]}" "clickhouse-client --multiquery" <<EOF
INSERT INTO default.inventory_planning_daily
SELECT
  event_date,
  'planning' AS sample_tier,
  toFloat32(0.02) AS sample_rate,
  ifNull(toString(colo_id), '') AS ssp_id,
  multiIf(
    url LIKE 'http%', 'site',
    assumeNotNull(url) != '', 'app',
    'unknown'
  ) AS inventory_type,
  cast(
    multiIf(
      has(splitByChar('|', ifNull(geo_ch, '')), '3612040'), 'RU',
      '  '
    ) AS FixedString(2)
  ) AS country,
  '' AS region,
  ifNull(toString(device), '') AS device_type,
  '' AS os,
  '' AS ad_format,
  ifNull(cs.name, ifNull(toString(size_id), '')) AS size,
  ifNull(toString(publisher_id), '') AS publisher_id,
  domain(ifNull(url, '')) AS domain,
  '' AS app_bundle,
  ifNull(
    nullIf(ssp_tag_id, ''),
    ifNull(toString(tag_id), '')
  ) AS tag_id,
  toUInt8(uid IS NOT NULL AND uid != '') AS user_known,
  '' AS deal_id,
  'RUB' AS bidfloor_cur,
  sumState(toUInt64(0)) AS bid_requests_smpl,
  uniqCombined64State(cityHash64(ifNull(uid, ''))) AS uniq_users_smpl,
  sumState(toUInt64(1)) AS imps,
  sumState(toFloat64(ifNull(bid_floor, 0))) AS bidfloor_sum,
  sumState(toUInt64(if(toFloat64(ifNull(bid_floor, 0)) > 0, 1, 0))) AS bidfloor_cnt,
  quantilesTDigestState(0.01, 0.05, 0.5)(toFloat64(ifNull(bid_floor, 0))) AS bidfloor_q,
  sumState(toFloat64(ifNull(win_price, 0))) AS win_price_sum,
  sumState(toUInt64(if(toFloat64(ifNull(win_price, 0)) > 0, 1, 0))) AS win_cnt,
  sumState(toFloat64(ifNull(win_price, 0))) AS spend_sum,
  sumState(toUInt64(0)) AS sampled_rows
FROM default.RImpression_recent AS r
LEFT JOIN default.creativesize AS cs ON cs.size_id = toInt32(r.size_id)
WHERE event_date = toDate('${DAY}')
  AND event_date NOT IN (
    SELECT DISTINCT event_date FROM default.inventory_planning_daily
    WHERE event_date = toDate('${DAY}')
  )
GROUP BY
  event_date,
  ssp_id,
  inventory_type,
  country,
  device_type,
  size,
  publisher_id,
  domain,
  tag_id,
  user_known;
EOF
done

echo "Done."
"${SSH[@]}" "clickhouse-client --query \"
SELECT event_date, sumMerge(imps) AS imps, count() AS keys
FROM default.inventory_planning_daily
GROUP BY event_date
ORDER BY event_date
\""
