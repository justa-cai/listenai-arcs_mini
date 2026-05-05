#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  echo "usage: $0 <branch> [base_ref]"
  exit 0
fi

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 <branch> [base_ref]" >&2
  exit 1
fi

branch="$1"
base_ref="${2:-${BASE_REF:-origin/master}}"
repo_root="$(git rev-parse --show-toplevel)"
project_root="${WORKTREE_ROOT:-$HOME/.config/superpowers/worktrees/$(basename "$repo_root")}"
sanitized_branch="${branch//\//-}"
path="$project_root/$sanitized_branch"

mkdir -p "$project_root"

if git show-ref --verify --quiet "refs/heads/$branch"; then
  git worktree add "$path" "$branch"
else
  git worktree add "$path" -b "$branch" "$base_ref"
fi

printf '%s\n' "$path"
