#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <mr_iid> <discussion_id> <body|@file>" >&2
  exit 1
fi

project_id="${PROJECT_ID:-2450}"
mr_iid="$1"
discussion_id="$2"
body_arg="$3"

if [[ "$body_arg" == @* ]]; then
  body_file="${body_arg#@}"
  body="$(cat "$body_file")"
else
  body="$body_arg"
fi

glab api \
  --method POST \
  "projects/$project_id/merge_requests/$mr_iid/discussions/$discussion_id/notes" \
  --raw-field "body=$body"
