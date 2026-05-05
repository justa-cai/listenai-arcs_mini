#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: $0 <iid> <title>" >&2
  exit 1
fi

iid="$1"
shift
title="$*"

slug_base="$(printf '%s' "$title" \
  | tr '[:upper:]' '[:lower:]' \
  | sed 's/[^a-z0-9]\+/-/g')"

slug="$(printf '%s' "$slug_base" \
  | sed 's/^-*//; s/-*$//' \
  | cut -c1-40)"

if [ -z "$slug" ]; then
  slug="issue"
fi

printf 'issue/%s-%s\n' "$iid" "$slug"
