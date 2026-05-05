---
name: gitlab-issue-pr-agent
description: Use when working in this repository and the user wants to fetch issues from cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk, solve them one by one, and create a dedicated branch, commit, and merge request linked to each issue.
---

# GitLab Issue PR Agent

This project-local skill automates the repeatable shell around issue-driven work for the ARCS SDK GitLab project.

## When To Use

- The user asks to fetch or process issues from `cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk`
- The user wants one branch and one merge request per issue
- The user wants merge requests linked back to the corresponding issue

## Fixed Defaults

- GitLab repo: `CSKG836746/arcs-sdk/arcs-sdk`
- Target branch: `master`
- Branch base: latest `origin/master`
- Worktree root: `~/.config/superpowers/worktrees/arcs-sdk`

## Workflow

1. List candidate issues with `bash scripts/issue_list.sh`
2. Read one issue in JSON with `glab api projects/2450/issues/<iid>` (see glab workarounds below)
3. Decide whether the issue is small enough for one branch and one MR
4. Prepare the issue branch with `bash scripts/prepare_issue_branch.sh <iid> "<title>"`
5. Work only inside the returned worktree path
6. Make the repo change and run the smallest convincing verification
7. Commit with `bash scripts/commit_message.sh`
8. Push the branch only after verification passes
9. Create a merge request with `bash scripts/create_mr.sh`

## Rules

- Never mix multiple issues into one branch
- Always run `git fetch origin master` before creating an issue branch
- Always create the issue branch from the latest `origin/master`
- Never create an issue branch from the current local `HEAD`
- Never open a merge request without verification evidence
- Never push an issue branch before verification passes
- If the workspace is dirty, do the issue work in a separate worktree
- If the issue is underspecified, stop and ask the user instead of inventing behavior

## glab Command Workarounds

**Problem:** `glab issue view <iid>` and `glab issue view <iid> --output json` both crash with segmentation fault on this GitLab instance (glab 1.74.0).

**Root cause:** Bug in glab's terminal rendering code (`issuable_view.go:287` - nil pointer dereference).

**Working alternatives:**

```bash
# Get issue details (project_id=2450 for CSKG836746/arcs-sdk/arcs-sdk)
glab api projects/2450/issues/<iid>

# Get issue comments
glab api projects/2450/issues/<iid>/notes

# Open issue in browser (works fine)
glab issue view <iid> --web

# List issues (works fine)
glab issue list

# Format API output with jq
glab api projects/2450/issues/48 | jq -r '.title, .description'
glab api projects/2450/issues/48/notes | jq -r '.[] | "[\(.author.username)] \(.body)"'
```

**Always use `glab api` instead of `glab issue view` when reading issue content programmatically.**

## Typical Commands

```bash
bash .project/skills/gitlab-issue-pr-agent/scripts/issue_list.sh
bash .project/skills/gitlab-issue-pr-agent/scripts/prepare_issue_branch.sh 37 "docs: docs/README.md 对英文文档支持的描述与仓库状态不一致"
bash .project/skills/gitlab-issue-pr-agent/scripts/commit_message.sh 37 "fix docs english support description"
bash .project/skills/gitlab-issue-pr-agent/scripts/create_mr.sh 37 issue/37-docs-readme-english-support "docs: align local docs guide with actual zh-only state"
```

## Verification Guidance

- Documentation-only issues: run a focused grep or file diff plus any relevant build command
- Build-system or code issues: run the narrowest test or build that proves the fix
- Report the exact command and outcome before claiming the issue is fixed
