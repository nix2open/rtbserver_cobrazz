-- Bid-price events (docs/BID_PRICE_SPEC.md §7). TTL 15 days.
CREATE TABLE IF NOT EXISTS default.bid_price_events
(
  event_time DateTime('UTC'),
  event_date Date MATERIALIZED toDate(event_time),
  request_id String,
  source_id LowCardinality(String) DEFAULT '',
  tag_id String DEFAULT '',
  size LowCardinality(String) DEFAULT '',
  format LowCardinality(String) DEFAULT '',
  country FixedString(2) DEFAULT '',
  device_type LowCardinality(String) DEFAULT '',
  at Int16 DEFAULT -1,
  bidfloor Float64 DEFAULT 0,
  original_price Float64 DEFAULT 0,
  model_price Float64 DEFAULT 0,
  has_model_price UInt8 DEFAULT 0,
  sent_price Float64 DEFAULT 0,
  mode LowCardinality(String) DEFAULT 'off',
  percent UInt8 DEFAULT 0,
  bucket LowCardinality(String) DEFAULT '',
  reason LowCardinality(String) DEFAULT '',
  won Int8 DEFAULT -1
)
ENGINE = MergeTree
PARTITION BY toYYYYMMDD(event_date)
ORDER BY (event_date, source_id, mode, request_id)
TTL event_date + INTERVAL 15 DAY DELETE
SETTINGS index_granularity = 8192;
