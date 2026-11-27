#!/usr/bin/python3.12
"""CSV adapter: BidPriceEvent FE log → bid_price_events."""

import csv
import sys

OUT_FIELDS = [
  'event_time',
  'request_id',
  'source_id',
  'tag_id',
  'size',
  'format',
  'country',
  'device_type',
  'at',
  'bidfloor',
  'original_price',
  'model_price',
  'has_model_price',
  'sent_price',
  'mode',
  'percent',
  'bucket',
  'reason',
  'won',
]


def normalize_country(value):
  v = (value or '').strip().upper()
  if v in ('RUS', 'RUSSIA'):
    return 'RU'
  if len(v) >= 2:
    return v[:2]
  return ''


def main(argv):
  writer = csv.writer(sys.stdout)
  writer.writerow(OUT_FIELDS)
  for path in argv[1:]:
    with open(path, 'r', encoding='utf-8', errors='replace') as fh:
      reader = csv.reader(fh)
      header = next(reader, None)
      if not header:
        continue
      for row in reader:
        if len(row) < 19:
          continue
        writer.writerow([
          row[0],
          row[1],
          row[2],
          row[3],
          row[4],
          row[5],
          normalize_country(row[6]),
          row[7],
          row[8] if row[8] != '' else '-1',
          row[9] or '0',
          row[10] or '0',
          row[11] or '0',
          row[12] or '0',
          row[13] or '0',
          row[14],
          row[15] or '0',
          row[16],
          row[17],
          row[18] if row[18] != '' else '-1',
        ])
  return 0


if __name__ == '__main__':
  raise SystemExit(main(sys.argv))
