from __future__ import annotations

import csv
import io
from datetime import date, timedelta
from typing import Any, Optional

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import HTMLResponse, ORJSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from pathlib import Path

from .ch import get_ch
from .config import Settings, get_settings

STATIC_DIR = Path(__file__).resolve().parents[1] / "static"

app = FastAPI(title="Bid price UI", default_response_class=ORJSONResponse)

ERROR_REASONS = (
  "deal",
  "not_first_price",
  "currency",
  "below_floor",
  "bad_price",
  "no_model",
  "inference_error",
)


def _period_bounds(period: str) -> tuple[date, date]:
  today = date.today()
  if period == "today":
    return today, today
  if period == "7d":
    return today - timedelta(days=6), today
  # default: yesterday
  y = today - timedelta(days=1)
  return y, y


def _filters_sql(
  source_id: Optional[str],
  mode: Optional[str],
) -> str:
  parts: list[str] = []
  if source_id:
    safe = source_id.replace("'", "\\'")
    parts.append(f"AND source_id = '{safe}'")
  if mode:
    safe = mode.replace("'", "\\'")
    parts.append(f"AND mode = '{safe}'")
  return " ".join(parts)


@app.get("/api/health")
def health() -> dict[str, Any]:
  settings = get_settings()
  ch = get_ch(settings)
  try:
    rows, ms = ch.query(
      f"SELECT count() AS c, max(event_date) AS d FROM {settings.clickhouse_table}"
    )
    row = rows[0] if rows else {"c": 0, "d": None}
    return {
      "ok": True,
      "rows": int(row.get("c") or 0),
      "max_event_date": str(row.get("d")) if row.get("d") is not None else None,
      "elapsed_ms": ms,
    }
  except Exception as exc:
    return {"ok": False, "error": str(exc)}


@app.get("/api/summary")
def summary(
  period: str = Query("yesterday", pattern="^(today|yesterday|7d)$"),
  source_id: Optional[str] = None,
  mode: Optional[str] = None,
) -> dict[str, Any]:
  settings = get_settings()
  ch = get_ch(settings)
  d0, d1 = _period_bounds(period)
  filt = _filters_sql(source_id, mode)
  table = settings.clickhouse_table
  sql = f"""
SELECT
  countIf(bucket = 'treated') AS treated_bids,
  countIf(bucket = 'holdout') AS holdout_bids,
  countIf(bucket = 'treated' AND sent_price != original_price) AS treated_changed,
  sumIf(original_price - sent_price,
        bucket = 'treated' AND won = 1 AND sent_price < original_price) AS saved,
  sumIf(sent_price - original_price,
        bucket = 'treated' AND won = 1 AND sent_price > original_price) AS overspend,
  countIf(bucket = 'treated' AND won = 1) AS treated_wins,
  countIf(bucket = 'holdout' AND won = 1) AS holdout_wins,
  countIf(won = -1) AS won_pending,
  countIf(reason IN {ERROR_REASONS}) AS errors
FROM {table}
WHERE event_date >= toDate('{d0}') AND event_date <= toDate('{d1}')
  {filt}
"""
  try:
    rows, ms = ch.query(sql)
  except Exception as exc:
    raise HTTPException(status_code=502, detail=str(exc)) from exc
  row = rows[0] if rows else {}
  treated = int(row.get("treated_bids") or 0)
  holdout = int(row.get("holdout_bids") or 0)
  if treated == 0 and holdout == 0:
    return {
      "period": period,
      "from": str(d0),
      "to": str(d1),
      "empty": True,
      "message": "нет данных",
      "elapsed_ms": ms,
      "cards": {},
    }
  pending = int(row.get("won_pending") or 0)
  cards = {
    "treated_bids": treated,
    "holdout_bids": holdout,
    "treated_changed": int(row.get("treated_changed") or 0),
    "saved": float(row.get("saved") or 0),
    "overspend": float(row.get("overspend") or 0),
    "win_rate_treated": (
      float(row.get("treated_wins") or 0) / treated if treated else 0.0
    ),
    "win_rate_holdout": (
      float(row.get("holdout_wins") or 0) / holdout if holdout else 0.0
    ),
    "errors": int(row.get("errors") or 0),
    "won_pending": pending,
    "saved_note": (
      "ещё нет сверки с показами" if pending == treated + holdout and pending > 0
      else None
    ),
  }
  return {
    "period": period,
    "from": str(d0),
    "to": str(d1),
    "empty": False,
    "elapsed_ms": ms,
    "cards": cards,
  }


@app.get("/api/by_source")
def by_source(
  period: str = Query("yesterday", pattern="^(today|yesterday|7d)$"),
  source_id: Optional[str] = None,
  mode: Optional[str] = None,
) -> dict[str, Any]:
  settings = get_settings()
  ch = get_ch(settings)
  d0, d1 = _period_bounds(period)
  filt = _filters_sql(source_id, mode)
  table = settings.clickhouse_table
  sql = f"""
SELECT
  source_id,
  any(mode) AS mode,
  any(percent) AS percent,
  countIf(bucket = 'treated') AS treated_bids,
  countIf(bucket = 'holdout') AS holdout_bids,
  countIf(bucket = 'treated' AND sent_price != original_price) AS treated_changed,
  sumIf(original_price - sent_price,
        bucket = 'treated' AND won = 1 AND sent_price < original_price) AS saved,
  sumIf(sent_price - original_price,
        bucket = 'treated' AND won = 1 AND sent_price > original_price) AS overspend,
  if(countIf(bucket = 'treated') = 0, 0,
     countIf(bucket = 'treated' AND won = 1) / countIf(bucket = 'treated'))
    AS win_rate_treated,
  if(countIf(bucket = 'holdout') = 0, 0,
     countIf(bucket = 'holdout' AND won = 1) / countIf(bucket = 'holdout'))
    AS win_rate_holdout,
  countIf(reason IN {ERROR_REASONS}) AS errors
FROM {table}
WHERE event_date >= toDate('{d0}') AND event_date <= toDate('{d1}')
  {filt}
GROUP BY source_id
ORDER BY treated_bids DESC
LIMIT 500
"""
  try:
    rows, ms = ch.query(sql)
  except Exception as exc:
    raise HTTPException(status_code=502, detail=str(exc)) from exc
  if not rows:
    return {
      "period": period,
      "empty": True,
      "message": "нет данных",
      "rows": [],
      "elapsed_ms": ms,
    }
  return {"period": period, "empty": False, "rows": rows, "elapsed_ms": ms}


@app.get("/api/errors")
def errors(
  period: str = Query("yesterday", pattern="^(today|yesterday|7d)$"),
  source_id: Optional[str] = None,
  mode: Optional[str] = None,
) -> dict[str, Any]:
  settings = get_settings()
  ch = get_ch(settings)
  d0, d1 = _period_bounds(period)
  filt = _filters_sql(source_id, mode)
  table = settings.clickhouse_table
  sql = f"""
SELECT reason, count() AS cnt
FROM {table}
WHERE event_date >= toDate('{d0}') AND event_date <= toDate('{d1}')
  AND reason IN {ERROR_REASONS}
  {filt}
GROUP BY reason
ORDER BY cnt DESC
"""
  try:
    rows, ms = ch.query(sql)
  except Exception as exc:
    raise HTTPException(status_code=502, detail=str(exc)) from exc
  return {"period": period, "rows": rows, "elapsed_ms": ms}


@app.get("/api/by_source.csv")
def by_source_csv(
  period: str = Query("yesterday", pattern="^(today|yesterday|7d)$"),
  source_id: Optional[str] = None,
  mode: Optional[str] = None,
):
  data = by_source(period=period, source_id=source_id, mode=mode)
  buf = io.StringIO()
  fields = [
    "source_id",
    "mode",
    "percent",
    "treated_bids",
    "holdout_bids",
    "treated_changed",
    "saved",
    "overspend",
    "win_rate_treated",
    "win_rate_holdout",
    "errors",
  ]
  writer = csv.DictWriter(buf, fieldnames=fields, extrasaction="ignore")
  writer.writeheader()
  for row in data.get("rows") or []:
    writer.writerow(row)
  buf.seek(0)
  return StreamingResponse(
    iter([buf.getvalue()]),
    media_type="text/csv",
    headers={"Content-Disposition": "attachment; filename=bid_price_by_source.csv"},
  )


@app.get("/", response_class=HTMLResponse)
def index() -> HTMLResponse:
  index_path = STATIC_DIR / "index.html"
  if not index_path.exists():
    return HTMLResponse("<h1>bid-price-ui</h1><p>static/index.html missing</p>")
  return HTMLResponse(index_path.read_text(encoding="utf-8"))


if STATIC_DIR.exists():
  app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")


def run() -> None:
  import uvicorn

  settings = get_settings()
  uvicorn.run(
    "app.main:app",
    host=settings.bind_host,
    port=settings.bind_port,
    reload=False,
  )


if __name__ == "__main__":
  run()
