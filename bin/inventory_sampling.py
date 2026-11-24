#!/usr/bin/python3
"""Inventory planning sampling helpers (§5.2).

Reads docs/inventory_sampling.yaml and provides deterministic keep()
plus rate lookup by tier. Used by batch ingest / future FE async logger.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

try:
  import yaml
except ImportError:  # pragma: no cover
  yaml = None


_DEFAULT_CONFIG = {
  'default_sample_rate': 0.02,
  'tiers': {
    'overview': {'rate': 0.02, 'dims': ['L0']},
    'planning': {'rate': 0.02, 'dims': ['L1']},
    'deep_dive': {'rate': 0.10, 'dims': ['L1'], 'scope': 'top_slices_only'},
    'full_audit': {'rate': 1.00, 'dims': ['L0'], 'ttl_days': 3},
  },
  'hash_key': 'request_id',
  'enabled': True,
  'wins_sample_rate': 1.0,
  'low_confidence_sampled_rows': 100,
  'control_drift_alert_pct': 3.0,
}


def default_config_path() -> Path:
  env = os.environ.get('INVENTORY_SAMPLING_CONFIG')
  if env:
    return Path(env)
  here = Path(__file__).resolve().parent
  return here.parent / 'docs' / 'inventory_sampling.yaml'


def load_sampling_config(path: Path | None = None) -> dict[str, Any]:
  cfg_path = path or default_config_path()
  if yaml is None or not cfg_path.is_file():
    return dict(_DEFAULT_CONFIG)
  with cfg_path.open('r', encoding='utf-8') as fh:
    loaded = yaml.safe_load(fh) or {}
  merged = dict(_DEFAULT_CONFIG)
  merged.update(loaded)
  if 'tiers' in loaded:
    tiers = dict(_DEFAULT_CONFIG['tiers'])
    tiers.update(loaded['tiers'] or {})
    merged['tiers'] = tiers
  return merged


def rate_for_tier(config: dict[str, Any], tier: str = 'planning') -> float:
  tiers = config.get('tiers') or {}
  if tier in tiers and isinstance(tiers[tier], dict):
    return float(tiers[tier].get('rate', config.get('default_sample_rate', 0.02)))
  return float(config.get('default_sample_rate', 0.02))


def city_hash64(data: bytes) -> int:
  """Minimal CityHash64-compatible stub via Python hash is NOT OK for CH parity.

  For batch jobs that must match ClickHouse cityHash64, prefer computing keep
  in SQL. This helper uses a stable FNV-1a 64-bit for local unit tests only.
  """
  h = 0xcbf29ce484222325
  for b in data:
    h ^= b
    h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
  return h


def keep_sample(request_id: str, sample_rate: float, hash_fn=city_hash64) -> bool:
  """keep = hash(request_id) % 1_000_000 < sample_rate * 1_000_000"""
  if sample_rate >= 1.0:
    return True
  if sample_rate <= 0.0:
    return False
  threshold = int(sample_rate * 1_000_000)
  return (hash_fn(request_id.encode('utf-8')) % 1_000_000) < threshold


def extrapolate_sum(sampled_count: float, sample_rate: float) -> float:
  if sample_rate <= 0:
    return 0.0
  return sampled_count / sample_rate


def low_confidence(sampled_rows: int, threshold: int | None = None) -> bool:
  thr = threshold if threshold is not None else _DEFAULT_CONFIG['low_confidence_sampled_rows']
  return sampled_rows < thr
