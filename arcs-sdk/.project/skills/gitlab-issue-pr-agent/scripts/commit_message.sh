#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: $0 <iid> <summary>" >&2
  exit 1
fi

iid="$1"
shift
summary="$*"

printf 'fix(issue-%s): %s\n\nRefs: #%s\n' "$iid" "$summary" "$iid"
