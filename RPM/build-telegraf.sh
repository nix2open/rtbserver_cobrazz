#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
VERSION=1.38.1
UPSTREAM_RELEASE=1
ARCH=x86_64
SOURCE_FILE="telegraf-$VERSION-$UPSTREAM_RELEASE.$ARCH.rpm"
SOURCE_PATH="/repos/oracle/8.6/REPO_EPEL8/$ARCH/Packages/$SOURCE_FILE"
DEFAULT_SOURCE_URL="http://192.168.10.1$SOURCE_PATH"
SOURCE_URL="${TELEGRAF_SOURCE_URL:-$DEFAULT_SOURCE_URL}"
# Upstream telegraf-1.38.1-1 used on cobrazz build hosts (dev02 seed).
SOURCE_SHA256=4e84312f1492c68cf50511e914f556733b01b3405835c01591836a6dafc216e1

RPM_SOURCES_DIR=$(rpm -E '%_sourcedir')
RPM_SPECS_DIR=$(rpm -E '%_specdir')
BIN_RPM_FOLDER=$(rpm -E '%_rpmdir/%_arch')
RESULT_RPMS="$SCRIPT_DIR/../build/RPMS"

mkdir -p "$RPM_SOURCES_DIR" "$RPM_SPECS_DIR" "$BIN_RPM_FOLDER" "$RESULT_RPMS"

seed_candidates_()
{
  # Prefer an already-seeded file; never block the build on the flaky mirror.
  local cand
  for cand in \
    "$RPM_SOURCES_DIR/$SOURCE_FILE" \
    "${TELEGRAF_SEED_RPM:-}" \
    "$HOME/RPM/SOURCES/$SOURCE_FILE" \
    /home/nikolay_fedorenko/RPM/SOURCES/"$SOURCE_FILE"
  do
    if [[ -n "$cand" && -f "$cand" ]]; then
      printf '%s\n' "$cand"
      return 0
    fi
  done
  return 1
}

ensure_telegraf_source_()
{
  local seed
  if seed=$(seed_candidates_); then
    if [[ "$seed" != "$RPM_SOURCES_DIR/$SOURCE_FILE" ]]; then
      cp -v "$seed" "$RPM_SOURCES_DIR/$SOURCE_FILE"
    else
      echo "Reusing existing $RPM_SOURCES_DIR/$SOURCE_FILE"
    fi
  else
    echo "telegraf source missing; trying $SOURCE_URL (short timeout)"
    # Mirror 192.168.10.1 often hangs at 0%. Fail fast instead of stalling the release.
    curl --fail --location \
      --connect-timeout 10 \
      --max-time 120 \
      --retry 2 \
      --retry-delay 2 \
      --output "$RPM_SOURCES_DIR/$SOURCE_FILE" \
      "$SOURCE_URL" || {
        echo "ERROR: cannot download $SOURCE_FILE from $SOURCE_URL" >&2
        echo "Seed it before build.sh, e.g.:" >&2
        echo "  sudo mkdir -p \"$RPM_SOURCES_DIR\"" >&2
        echo "  sudo cp -v \$HOME/RPM/SOURCES/$SOURCE_FILE \"$RPM_SOURCES_DIR/\"" >&2
        exit 1
      }
  fi

  if ! echo "$SOURCE_SHA256  $RPM_SOURCES_DIR/$SOURCE_FILE" | sha256sum --check --strict
  then
    echo "ERROR: sha256 mismatch for $RPM_SOURCES_DIR/$SOURCE_FILE" >&2
    echo "Expected: $SOURCE_SHA256" >&2
    sha256sum "$RPM_SOURCES_DIR/$SOURCE_FILE" >&2 || true
    echo "Replace the seed RPM or update SOURCE_SHA256 in RPM/build-telegraf.sh" >&2
    exit 1
  fi
}

ensure_telegraf_source_()

cp "$SCRIPT_DIR/SPECS/telegraf.spec" "$RPM_SPECS_DIR/foros-telegraf.spec"
rpmbuild --force -bb \
  --define "version $VERSION" \
  --define "telegraf_upstream_release $UPSTREAM_RELEASE" \
  "$RPM_SPECS_DIR/foros-telegraf.spec"

cp "$BIN_RPM_FOLDER"/foros-telegraf-"$VERSION"-*."$ARCH".rpm "$RESULT_RPMS/"
