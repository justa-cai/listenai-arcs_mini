#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  echo "usage: $0 <iid> <title>"
  exit 0
fi

if [ "$#" -lt 2 ]; then
  echo "usage: $0 <iid> <title>" >&2
  exit 1
fi

repo_root="$(git rev-parse --show-toplevel)"
iid="$1"
shift
title="$*"

git -C "$repo_root" fetch origin master

branch="$(bash "$repo_root/.project/skills/gitlab-issue-pr-agent/scripts/branch_name.sh" "$iid" "$title")"

bash "$repo_root/.project/skills/gitlab-issue-pr-agent/scripts/create_worktree.sh" "$branch" origin/master
