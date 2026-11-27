from __future__ import annotations

import threading
import time
from typing import Any

import clickhouse_connect

from .config import Settings


class ClickHouse:
  def __init__(self, settings: Settings):
    self.settings = settings
    self._local = threading.local()

  def client(self):
    client = getattr(self._local, "client", None)
    if client is None:
      client = clickhouse_connect.get_client(
        host=self.settings.clickhouse_host,
        port=self.settings.clickhouse_port,
        username=self.settings.clickhouse_user,
        password=self.settings.clickhouse_password or "",
        database=self.settings.clickhouse_database,
        connect_timeout=10,
        send_receive_timeout=self.settings.query_timeout_sec,
      )
      self._local.client = client
    return client

  def query(self, sql: str) -> tuple[list[dict[str, Any]], int]:
    started = time.perf_counter()
    result = self.client().query(sql)
    elapsed_ms = int((time.perf_counter() - started) * 1000)
    cols = list(result.column_names)
    rows = [dict(zip(cols, row)) for row in result.result_rows]
    return rows, elapsed_ms


_ch: ClickHouse | None = None


def get_ch(settings: Settings) -> ClickHouse:
  global _ch
  if _ch is None:
    _ch = ClickHouse(settings)
  return _ch
