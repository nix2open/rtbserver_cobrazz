#!/usr/bin/env bash
# Bid-price ops control (no rebuild / no service restart required).
#
# Usage:
#   bid_price_ctl.sh status
#   bid_price_ctl.sh off
#   bid_price_ctl.sh on
#   bid_price_ctl.sh show
#
set -euo pipefail

DISABLE_FILE="${BID_PRICE_DISABLE_FILE:-/opt/foros/server/var/run/bid_price.disable}"
CONFIG_FILE="${BID_PRICE_CONFIG_FILE:-/opt/foros/server/etc/bid_price.json}"
OUT_RAM="${BID_PRICE_OUT:-/opt/foros/server/var/log/FCGIRtbServer/Out/BidPriceEvent.ram}"

cmd="${1:-status}"

case "$cmd" in
  status)
    if [[ -e "$DISABLE_FILE" ]]; then
      echo "disable: ON ($DISABLE_FILE exists) → global off"
    else
      echo "disable: OFF"
    fi
    if [[ -f "$CONFIG_FILE" ]]; then
      echo "config: $CONFIG_FILE"
      head -c 2000 "$CONFIG_FILE" || true
      echo
    else
      echo "config: missing ($CONFIG_FILE)"
    fi
    if [[ -d "$OUT_RAM" ]]; then
      n=$(find "$OUT_RAM" -maxdepth 1 -type f -name 'BidPriceEvent_*' 2>/dev/null | wc -l | tr -d ' ')
      echo "out files: $n in $OUT_RAM"
    else
      echo "out: $OUT_RAM (missing)"
    fi
    ;;
  off)
    mkdir -p "$(dirname "$DISABLE_FILE")"
    touch "$DISABLE_FILE"
    echo "OK: touched $DISABLE_FILE (global off, no restart)"
    ;;
  on)
    rm -f "$DISABLE_FILE"
    echo "OK: removed $DISABLE_FILE"
    ;;
  show)
    if [[ -f "$CONFIG_FILE" ]]; then
      cat "$CONFIG_FILE"
    else
      echo "missing: $CONFIG_FILE" >&2
      exit 1
    fi
    ;;
  *)
    echo "Usage: $0 {status|off|on|show}" >&2
    exit 2
    ;;
esac
