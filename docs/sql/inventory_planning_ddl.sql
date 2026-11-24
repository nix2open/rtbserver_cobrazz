-- Inventory Planning cube DDL (§6 INVENTORY_PLANNING_SPEC)
-- Target: click00 / default
-- Apply: clickhouse-client --multiquery < docs/sql/inventory_planning_ddl.sql

CREATE TABLE IF NOT EXISTS default.inventory_planning_daily
(
  event_date Date,
  sample_tier LowCardinality(String) DEFAULT 'planning',
  sample_rate Float32 DEFAULT 0.02,
  ssp_id LowCardinality(String) DEFAULT '',
  inventory_type LowCardinality(String) DEFAULT 'unknown',
  country FixedString(2) DEFAULT '',
  region LowCardinality(String) DEFAULT '',
  device_type LowCardinality(String) DEFAULT '',
  os LowCardinality(String) DEFAULT '',
  ad_format LowCardinality(String) DEFAULT '',
  size LowCardinality(String) DEFAULT '',
  publisher_id String DEFAULT '',
  domain String DEFAULT '',
  app_bundle String DEFAULT '',
  tag_id String DEFAULT '',
  user_known UInt8 DEFAULT 0,
  deal_id String DEFAULT '',
  bidfloor_cur LowCardinality(String) DEFAULT 'RUB',
  bid_requests_smpl AggregateFunction(sum, UInt64),
  uniq_users_smpl AggregateFunction(uniqCombined64, UInt64),
  imps AggregateFunction(sum, UInt64),
  bidfloor_sum AggregateFunction(sum, Float64),
  bidfloor_cnt AggregateFunction(sum, UInt64),
  bidfloor_q AggregateFunction(quantilesTDigest(0.01, 0.05, 0.5), Float64),
  win_price_sum AggregateFunction(sum, Float64),
  win_cnt AggregateFunction(sum, UInt64),
  spend_sum AggregateFunction(sum, Float64),
  sampled_rows AggregateFunction(sum, UInt64)
)
ENGINE = AggregatingMergeTree
PARTITION BY toYYYYMM(event_date)
ORDER BY (
  event_date,
  sample_tier,
  ssp_id,
  inventory_type,
  country,
  device_type,
  ad_format,
  size,
  os,
  region,
  domain,
  app_bundle,
  publisher_id,
  tag_id,
  user_known,
  deal_id,
  bidfloor_cur,
  sample_rate
)
TTL event_date + INTERVAL 45 DAY DELETE
SETTINGS index_granularity = 8192;

-- Spec §6.1 shows SummingMergeTree + plain UInt64; uniq needs AggregateFunction,
-- so AggregatingMergeTree with sumState for bid_requests (same merge semantics).
CREATE TABLE IF NOT EXISTS default.inventory_control_daily
(
  event_date Date,
  ssp_id LowCardinality(String) DEFAULT '',
  inventory_type LowCardinality(String) DEFAULT 'unknown',
  country FixedString(2) DEFAULT '',
  ad_format LowCardinality(String) DEFAULT '',
  bid_requests AggregateFunction(sum, UInt64),
  uniq_users AggregateFunction(uniqCombined64, UInt64)
)
ENGINE = AggregatingMergeTree
PARTITION BY toYYYYMM(event_date)
ORDER BY (event_date, ssp_id, inventory_type, country, ad_format)
TTL event_date + INTERVAL 45 DAY DELETE;

-- NOTE: outer SELECT renames aliases — CH rejects sumMerge(imps) AS imps
-- (alias collides with AggregateFunction column name).
CREATE OR REPLACE VIEW default.v_inventory_planning_daily AS
SELECT
  event_date,
  sample_tier,
  sample_rate,
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
  bid_requests_extrapolated AS bid_requests,
  uniq_users_sampled,
  imps_cnt AS imps,
  avg_bidfloor_cpm,
  bidfloor_p01_p05_p50,
  avg_win_cpm,
  spend,
  sampled_rows_cnt AS sampled_rows,
  low_confidence
FROM
(
  SELECT
    event_date,
    sample_tier,
    sample_rate,
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
    toUInt64(sumMerge(bid_requests_smpl) / greatest(sample_rate, CAST(0.000001 AS Float32))) AS bid_requests_extrapolated,
    uniqCombined64Merge(uniq_users_smpl) AS uniq_users_sampled,
    sumMerge(imps) AS imps_cnt,
    if(sumMerge(bidfloor_cnt) = 0, NULL,
       sumMerge(bidfloor_sum) / sumMerge(bidfloor_cnt)) AS avg_bidfloor_cpm,
    quantilesTDigestMerge(0.01, 0.05, 0.5)(bidfloor_q) AS bidfloor_p01_p05_p50,
    if(sumMerge(win_cnt) = 0, NULL,
       sumMerge(win_price_sum) / sumMerge(win_cnt)) AS avg_win_cpm,
    sumMerge(spend_sum) AS spend,
    sumMerge(sampled_rows) AS sampled_rows_cnt,
    if(sumMerge(sampled_rows) < 100, toUInt8(1), toUInt8(0)) AS low_confidence
  FROM default.inventory_planning_daily
  GROUP BY
    event_date,
    sample_tier,
    sample_rate,
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
    bidfloor_cur
);

CREATE OR REPLACE VIEW default.v_inventory_control_daily AS
SELECT
  event_date,
  ssp_id,
  inventory_type,
  country,
  ad_format,
  br AS bid_requests,
  uu AS uniq_users
FROM
(
  SELECT
    event_date,
    ssp_id,
    inventory_type,
    country,
    ad_format,
    sumMerge(bid_requests) AS br,
    uniqCombined64Merge(uniq_users) AS uu
  FROM default.inventory_control_daily
  GROUP BY event_date, ssp_id, inventory_type, country, ad_format
);
