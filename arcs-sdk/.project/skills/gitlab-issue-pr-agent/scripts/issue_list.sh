#!/usr/bin/env bash
set -euo pipefail

repo="${1:-CSKG836746/arcs-sdk/arcs-sdk}"
state="${ISSUE_STATE:-opened}"
per_page="${ISSUE_PER_PAGE:-100}"

glab issue list \
  --repo "$repo" \
  --all \
  --per-page "$per_page" \
  --output json \
  | jq --arg state "$state" 'map({
      iid,
      title,
      state,
      labels: (.labels // []),
      created_at,
      updated_at,
      web_url
    }) | map(select(.state == $state))'
