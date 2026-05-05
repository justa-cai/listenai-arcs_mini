#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <mr_iid>" >&2
  exit 1
fi

project_id="${PROJECT_ID:-2450}"
mr_iid="$1"

if [ -n "${MR_DETAIL_JSON:-}" ]; then
  mr_json="$MR_DETAIL_JSON"
else
  mr_json="$(glab api "projects/$project_id/merge_requests/$mr_iid")"
fi

printf '%s\n' "$mr_json" | jq --argjson mr_iid "$mr_iid" '
  .head_pipeline as $p |
  {
    mr_iid: $mr_iid,
    pipeline: (
      if $p == null then
        { status: null, message: "No pipeline found for this MR" }
      else
        {
          id: $p.id,
          status: $p.status,
          ref: $p.ref,
          sha: ($p.sha // null | .[:8]),
          web_url: ($p.web_url // null),
          created_at: ($p.created_at // null),
          updated_at: ($p.updated_at // null)
        }
      end
    ),
    mergeable: (
      if $p == null then null
      elif $p.status == "success" then true
      elif $p.status == "failed" then false
      else null
      end
    )
  }
'
