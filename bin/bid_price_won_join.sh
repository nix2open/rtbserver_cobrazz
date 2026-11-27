#!/bin/bash
# Hourly won join: set won=1 when request_id is in RImpression_recent.
# Restrict both sides to recent partitions so the mutation stays light.
set -euo pipefail

CH_CONN="${BID_PRICE_CH_CONN:--h click00}"
LOOKBACK_DAYS="${BID_PRICE_WON_LOOKBACK_DAYS:-2}"

clickhouse-client ${CH_CONN} --multiquery <<SQL
CREATE TABLE IF NOT EXISTS default.bid_price_won_staging
(
  request_id String
) ENGINE = Memory;

TRUNCATE TABLE default.bid_price_won_staging;

INSERT INTO default.bid_price_won_staging
SELECT DISTINCT b.request_id
FROM default.bid_price_events AS b
INNER JOIN default.RImpression_recent AS r
  ON b.request_id = r.request_id
WHERE b.won = -1
  AND b.event_date >= today() - ${LOOKBACK_DAYS}
  AND r.event_date >= today() - ${LOOKBACK_DAYS} - 1;

ALTER TABLE default.bid_price_events
UPDATE won = 1
WHERE won = -1
  AND event_date >= today() - ${LOOKBACK_DAYS}
  AND request_id IN (SELECT request_id FROM default.bid_price_won_staging);

ALTER TABLE default.bid_price_events
UPDATE won = 0
WHERE won = -1
  AND event_date >= today() - ${LOOKBACK_DAYS}
  AND event_time < now() - INTERVAL 2 HOUR;

DROP TABLE IF EXISTS default.bid_price_won_staging;
SQL

echo "OK: bid_price_events won join lookback=${LOOKBACK_DAYS}d"
