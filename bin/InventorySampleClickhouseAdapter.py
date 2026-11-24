#!/usr/bin/python3
"""CSV adapter: InventorySample FE log → inventory_events_sample columns."""

import csv
import sys

# Header from LogCommons/InventorySample.hpp csv_header()
# Timestamp,RequestID,SampleRate,SampleTier,SSP,InventoryType,Country,Region,
# DeviceType,OS,AdFormat,Size,PublisherID,Domain,AppBundle,TagID,UserKnown,
# DealID,BidfloorCur,Bidfloor,UidHash

OUT_FIELDS = [
  'event_time',
  'request_id',
  'sample_rate',
  'sample_tier',
  'ssp_id',
  'inventory_type',
  'country',
  'region',
  'device_type',
  'os',
  'ad_format',
  'size',
  'publisher_id',
  'domain',
  'app_bundle',
  'tag_id',
  'user_known',
  'deal_id',
  'bidfloor_cur',
  'bidfloor',
  'uid_hash',
]


def normalize_country(value: str) -> str:
  v = (value or '').strip().upper()
  if len(v) >= 2:
    if v == 'RUS':
      return 'RU'
    return v[:2]
  return '  '


def normalize_domain(value: str) -> str:
  v = (value or '').strip().lower()
  if '://' in v:
    v = v.split('://', 1)[1]
  v = v.split('/', 1)[0]
  if ':' in v:
    v = v.split(':', 1)[0]
  if v.startswith('www.'):
    v = v[4:]
  return v


def main(argv: list[str]) -> int:
  writer = csv.writer(sys.stdout)
  writer.writerow(OUT_FIELDS)
  for path in argv[1:]:
    with open(path, 'r', encoding='utf-8', errors='replace') as fh:
      reader = csv.reader(fh)
      header = next(reader, None)
      if not header:
        continue
      for row in reader:
        if len(row) < 21:
          continue
        country = normalize_country(row[6])
        writer.writerow([
          row[0],  # event_time
          row[1],
          row[2],
          row[3],
          row[4],
          row[5],
          country,
          row[7],
          row[8],
          row[9],
          row[10],
          row[11],
          row[12],
          normalize_domain(row[13]),
          row[14],
          row[15],
          row[16],
          row[17],
          row[18] or 'RUB',
          row[19],
          row[20],
        ])
  return 0


if __name__ == '__main__':
  raise SystemExit(main(sys.argv))
