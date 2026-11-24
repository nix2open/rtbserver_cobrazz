-- 5 эталонных SQL (Phase 1 §9) — GetIntent-like yesterday web / planning slices
-- Source: default.v_inventory_planning_daily

-- 1) Yesterday web: volume + floors + win CPM
SELECT
  sum(bid_requests) AS bid_requests,
  sum(uniq_users_sampled) AS uniq_users_sampled,
  sum(imps) AS imps,
  avg(avg_win_cpm) AS avg_win_cpm,
  sum(sampled_rows) AS sampled_rows,
  max(low_confidence) AS low_confidence
FROM default.v_inventory_planning_daily
WHERE event_date = yesterday()
  AND inventory_type = 'site';

-- 2) Yesterday web by domain (top 20 by imps)
SELECT
  domain,
  sum(imps) AS imps,
  sum(bid_requests) AS bid_requests,
  avg(avg_win_cpm) AS avg_win_cpm,
  any(bidfloor_p01_p05_p50).2 AS bidfloor_p05
FROM default.v_inventory_planning_daily
WHERE event_date = yesterday()
  AND inventory_type = 'site'
  AND country = 'RU'
GROUP BY domain
ORDER BY imps DESC
LIMIT 20;

-- 3) Last 7 days: SSP × device × size
SELECT
  event_date,
  ssp_id,
  device_type,
  size,
  sum(imps) AS imps,
  sum(bid_requests) AS bid_requests,
  avg(avg_win_cpm) AS avg_win_cpm
FROM default.v_inventory_planning_daily
WHERE event_date >= today() - 7
  AND event_date < today()
GROUP BY event_date, ssp_id, device_type, size
ORDER BY event_date, imps DESC
LIMIT 100;

-- 4) Cheap inventory: low win CPM with volume (RU site)
SELECT
  domain,
  size,
  sum(imps) AS imps,
  avg(avg_win_cpm) AS avg_win_cpm,
  any(bidfloor_p01_p05_p50).2 AS bidfloor_p05
FROM default.v_inventory_planning_daily
WHERE event_date >= today() - 7
  AND event_date < today()
  AND country = 'RU'
  AND inventory_type = 'site'
GROUP BY domain, size
HAVING imps >= 1000
ORDER BY avg_win_cpm ASC
LIMIT 50;

-- 5) Drift check vs control L0 (requires bid-sample + control ingest)
SELECT
  p.event_date,
  p.bid_requests AS extrapolated_l0,
  c.bid_requests AS control_l0,
  if(c.bid_requests = 0, NULL,
     abs(1 - p.bid_requests / c.bid_requests)) AS abs_drift
FROM
(
  SELECT
    event_date,
    sum(bid_requests) AS bid_requests
  FROM default.v_inventory_planning_daily
  WHERE event_date >= today() - 7
  GROUP BY event_date
) AS p
LEFT JOIN
(
  SELECT
    event_date,
    sum(bid_requests) AS bid_requests
  FROM default.v_inventory_control_daily
  WHERE event_date >= today() - 7
  GROUP BY event_date
) AS c USING (event_date)
ORDER BY event_date;
