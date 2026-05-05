#!/usr/bin/env bash
set -euo pipefail

repo="${REPO:-CSKG836746/arcs-sdk/arcs-sdk}"
per_page="${MR_PER_PAGE:-100}"
current_username="${GITLAB_USERNAME:-$(glab api user | jq -r '.username')}"

if [ -n "${MR_LIST_JSON:-}" ]; then
  mr_json="$MR_LIST_JSON"
else
  mr_json="$(
    glab mr list \
      --repo "$repo" \
      --all \
      --per-page "$per_page" \
      --output json
  )"
fi

printf '%s\n' "$mr_json" | jq --arg username "$current_username" '
  map(select(.state == "opened")) |
  map(
    . as $mr |
    .roles = (
      [
        (if ($mr.author.username // "") == $username then "author" else empty end),
        (if any(($mr.assignees // [])[]?; .username == $username) then "assignee" else empty end),
        (if any(($mr.reviewers // [])[]?; .username == $username) then "reviewer" else empty end)
      ]
    )
  ) |
  map(select((.roles | length) > 0)) |
  map({
    iid,
    title,
    draft,
    state,
    roles,
    pipeline_status: (.head_pipeline.status // null),
    author: (.author.username // null),
    assignees: [(.assignees // [])[]?.username],
    reviewers: [(.reviewers // [])[]?.username],
    web_url,
    updated_at
  }) |
  sort_by(.updated_at) |
  reverse
'
