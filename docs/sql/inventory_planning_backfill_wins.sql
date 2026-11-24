-- Backfill inventory_planning_daily from RImpression_recent (wins path, 100% imps).
-- sample_tier=planning, sample_rate from config (0.02) so states share the planning key;
-- imps/win metrics are NOT extrapolated in the view; bid_requests_smpl stays 0 until
-- async bid-sample ingest is live.
--
-- Usage:
--   clickhouse-client --param_day='2026-11-23' --multiquery < this file
-- Or set day inline below.

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
      ''
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
  sumState(toFloat64(if(ifNull(bid_floor, 0) > 0, bid_floor, 0))) AS bidfloor_sum,
  sumState(toUInt64(if(ifNull(bid_floor, 0) > 0, 1, 0))) AS bidfloor_cnt,
  quantilesTDigestState(0.01, 0.05, 0.5)(
    if(ifNull(bid_floor, 0) > 0, toFloat64(bid_floor), CAST(NULL AS Nullable(Float64)))
  ) AS bidfloor_q,
  sumState(toFloat64(ifNull(win_price, 0))) AS win_price_sum,
  sumState(toUInt64(if(ifNull(win_price, 0) > 0, 1, 0))) AS win_cnt,
  sumState(toFloat64(ifNull(win_price, 0))) AS spend_sum,
  sumState(toUInt64(0)) AS sampled_rows
FROM default.RImpression_recent AS r
LEFT JOIN default.creativesize AS cs ON cs.size_id = toInt32(r.size_id)
WHERE event_date = {day:Date}
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
