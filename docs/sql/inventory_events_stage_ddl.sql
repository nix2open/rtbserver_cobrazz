-- Short-retention raw/stage for sampled bid requests (§4.1 / §6.4)
-- TTL 14d; feeds nightly/batch into inventory_planning_daily + control L0.

CREATE TABLE IF NOT EXISTS default.inventory_events_sample
(
  event_time DateTime('UTC'),
  event_date Date MATERIALIZED toDate(event_time),
  request_id String,
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
  bidfloor Float64 DEFAULT 0,
  uid_hash UInt64 DEFAULT 0
)
ENGINE = MergeTree
PARTITION BY toYYYYMMDD(event_date)
ORDER BY (event_date, ssp_id, inventory_type, country, domain, request_id)
TTL event_date + INTERVAL 14 DAY DELETE
SETTINGS index_granularity = 8192;
