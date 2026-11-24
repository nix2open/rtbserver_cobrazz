#!/bin/bash
# InventorySample ops control (no rebuild / no service restart required).
# Reads/writes files that BiddingFrontendLogger polls on flush (~Geo period).
#
# Usage (on FE as root or aduser with write perms):
#   inventory_sample_ctl.sh status
#   inventory_sample_ctl.sh off
#   inventory_sample_ctl.sh on
#   inventory_sample_ctl.sh rate 0.02
#   inventory_sample_ctl.sh rate 0
#   inventory_sample_ctl.sh purge
#   inventory_sample_ctl.sh space

set -euo pipefail

DISABLE_FILE="${INVENTORY_SAMPLE_DISABLE_FILE:-/opt/foros/server/var/run/inventory_sample.disable}"
RATE_FILE="${INVENTORY_SAMPLE_RATE_FILE:-/opt/foros/server/etc/inventory_sample.rate}"
OUT_RAM="${INVENTORY_SAMPLE_OUT:-/opt/foros/server/var/log/FCGIRtbServer/Out/InventorySample.ram}"
OUT_GEO_LEGACY="${INVENTORY_SAMPLE_GEO_LEGACY:-/opt/foros/server/var/log/FCGIRtbServer/Out/Geo.ram}"
RAM_FS="${INVENTORY_SAMPLE_RAM_FS:-/dev/rtbserver_ram_fs}"

usage() {
  sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

need_write_dir() {
  local dir
  dir=$(dirname "$1")
  mkdir -p "$dir"
}

cmd_status() {
  echo "disable_file: $DISABLE_FILE"
  if [[ -e "$DISABLE_FILE" ]]; then
    echo "collection: OFF (disable file present)"
  else
    echo "collection: ON (no disable file)"
  fi

  if [[ -f "$RATE_FILE" ]]; then
    echo "rate_file: $RATE_FILE = $(tr -d '[:space:]' < "$RATE_FILE")"
  else
    echo "rate_file: $RATE_FILE (absent → binary default, usually 0.02)"
  fi

  echo "out_dir: $OUT_RAM"
  if [[ -d "$OUT_RAM" ]]; then
    local n size
    n=$(find "$OUT_RAM" -maxdepth 1 -type f -name 'InventorySample_*' 2>/dev/null | wc -l | tr -d ' ')
    size=$(du -sh "$OUT_RAM" 2>/dev/null | awk '{print $1}')
    echo "pending_files: $n  size: $size"
  else
    echo "pending_files: (out dir missing)"
  fi

  if [[ -d "$OUT_GEO_LEGACY" ]]; then
    local legacy
    legacy=$(find "$OUT_GEO_LEGACY" -maxdepth 1 -type f -name 'InventorySample_*' 2>/dev/null | wc -l | tr -d ' ')
    if [[ "$legacy" != "0" ]]; then
      echo "WARN: legacy InventorySample_* in Geo.ram: $legacy (purge recommended)"
    fi
  fi
}

cmd_off() {
  need_write_dir "$DISABLE_FILE"
  touch "$DISABLE_FILE"
  echo "OK: collection disabled ($DISABLE_FILE)"
  echo "Takes effect on next logger flush (~10s). No restart needed."
}

cmd_on() {
  rm -f "$DISABLE_FILE"
  echo "OK: collection enabled (disable file removed)"
  echo "Takes effect on next logger flush (~10s). No restart needed."
}

cmd_rate() {
  local rate="${1:-}"
  if [[ -z "$rate" ]]; then
    echo "usage: $0 rate <0..1>" >&2
    exit 2
  fi
  # basic float check
  if ! awk -v r="$rate" 'BEGIN { exit !(r+0==r && r>=0 && r<=1) }'; then
    echo "rate must be a number in [0, 1], got: ${rate}" >&2
    exit 2
  fi
  need_write_dir "$RATE_FILE"
  printf '%s\n' "$rate" > "$RATE_FILE"
  echo "OK: rate set to $rate ($RATE_FILE)"
  if awk -v r="$rate" 'BEGIN { exit !(r==0) }'; then
    echo "rate=0 → no samples (equivalent to off for sampling)."
  fi
  echo "Takes effect on next logger flush (~10s). No restart needed."
}

cmd_purge() {
  local removed=0
  for dir in "$OUT_RAM" "$OUT_GEO_LEGACY" \
    /dev/rtbserver_ram_fs/log/FCGIRtbServer/Out/InventorySample \
    /dev/rtbserver_ram_fs/log/FCGIRtbServer/Out/Geo
  do
    [[ -d "$dir" ]] || continue
    while IFS= read -r -d '' f; do
      rm -f "$f"
      removed=$((removed + 1))
    done < <(find "$dir" -maxdepth 1 -type f -name 'InventorySample_*' -print0 2>/dev/null)
  done
  echo "OK: removed $removed InventorySample_* file(s)"
}

cmd_space() {
  df -h "$RAM_FS" 2>/dev/null || df -h /dev/rtbserver_ram_fs 2>/dev/null || true
  df -h /opt/foros/server/var 2>/dev/null || true
  for dir in "$OUT_RAM" "$OUT_GEO_LEGACY" \
    /dev/rtbserver_ram_fs/log/FCGIRtbServer/Out/InventorySample \
    /dev/rtbserver_ram_fs/log/FCGIRtbServer/Out/Geo
  do
    [[ -e "$dir" ]] || continue
    du -sh "$dir" 2>/dev/null || true
  done
}

main() {
  local cmd="${1:-status}"
  shift || true
  case "$cmd" in
    status) cmd_status ;;
    off|disable|stop) cmd_off ;;
    on|enable|start) cmd_on ;;
    rate) cmd_rate "${1:-}" ;;
    purge|flush) cmd_purge ;;
    space|df) cmd_space ;;
    -h|--help|help) usage 0 ;;
    *) echo "unknown command: $cmd" >&2; usage 2 ;;
  esac
}

main "$@"
