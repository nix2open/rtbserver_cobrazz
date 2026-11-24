-- Roll sampled bid events into inventory_planning_daily (planning tier).
-- Run after InventorySample lands in inventory_events_sample.
-- Usage: clickhouse-client --param_day='2026-11-24' < this file
--   or set day inline.

INSERT INTO default.inventory_planning_daily
SELECT
  event_date,
  any(sample_tier) AS sample_tier,
  any(sample_rate) AS sample_rate,
  ssp_id,
  inventory_type,
  country,
  region,
  device_type,
  os,
  ad_format,
  size,
  publisher_id,
  domain,
  app_bundle,
  tag_id,
  user_known,
  deal_id,
  bidfloor_cur,
  sumState(toUInt64(1)) AS bid_requests_smpl,
  uniqCombined64State(uid_hash) AS uniq_users_smpl,
  sumState(toUInt64(0)) AS imps,
  sumState(toFloat64(if(bidfloor > 0, bidfloor, 0))) AS bidfloor_sum,
  sumState(toUInt64(if(bidfloor > 0, 1, 0))) AS bidfloor_cnt,
  quantilesTDigestState(0.01, 0.05, 0.5)(toFloat64(if(bidfloor > 0, bidfloor, 0))) AS bidfloor_q,
  sumState(toFloat64(0)) AS win_price_sum,
  sumState(toUInt64(0)) AS win_cnt,
  sumState(toFloat64(0)) AS spend_sum,
  sumState(toUInt64(1)) AS sampled_rows
FROM default.inventory_events_sample
WHERE event_date = {day:Date}
GROUP BY
  event_date,
  ssp_id,
  inventory_type,
  country,
  region,
  device_type,
  os,
  ad_format,
  size,
  publisher_id,
  domain,
  app_bundle,
  tag_id,
  user_known,
  deal_id,
  bidfloor_cur;

-- L0 control from same sample day is NOT 100%; for control use FE counters or
-- full_audit tier. Optional coarse insert for ops visibility:
INSERT INTO default.inventory_control_daily
SELECT
  event_date,
  ssp_id,
  inventory_type,
  country,
  ad_format,
  sumState(toUInt64(toUInt64(1) / greatest(sample_rate, 0.000001))) AS bid_requests,
  uniqCombined64State(uid_hash) AS uniq_users
FROM default.inventory_events_sample
WHERE event_date = {day:Date}
GROUP BY event_date, ssp_id, inventory_type, country, ad_format;
