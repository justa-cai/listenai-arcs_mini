#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <iid> <source_branch> <title> [target_branch]" >&2
  exit 1
fi

repo="${REPO:-CSKG836746/arcs-sdk/arcs-sdk}"
iid="$1"
source_branch="$2"
title="$3"
target_branch="${4:-${TARGET_BRANCH:-master}}"

description=$(cat <<EOF
Closes #$iid

Automated by project skill \`gitlab-issue-pr-agent\`.
EOF
)

url=$(
  glab mr create \
    --repo "$repo" \
    --source-branch "$source_branch" \
    --target-branch "$target_branch" \
    --title "$title" \
    --description "$description" \
    --yes
)

glab mr update "$source_branch" \
  --repo "$repo" \
  --ready \
  --yes >/dev/null

printf '%s\n' "$url"
