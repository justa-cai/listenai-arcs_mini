---
name: gitlab-mr-review-followup-agent
description: Use when working in this repository and the user wants to manually check GitLab MR review feedback, summarize unresolved discussions, reply in threads, and drive the post-MR review/fix/re-verify loop.
---

# GitLab MR Review Follow-up Agent

This project-local skill covers the manual follow-up loop after a merge request already exists.

## When To Use

- The user wants to check review feedback on opened MRs in `cloud.listenai.com/CSKG836746/arcs-sdk/arcs-sdk`
- The user wants a pending-review summary for one MR
- The user wants to reply in a review thread after a fix or clarification

## Fixed Defaults

- GitLab repo: `CSKG836746/arcs-sdk/arcs-sdk`
- GitLab project id: `2450`
- Target state: opened MRs only

## Workflow

1. List related opened MRs with `bash scripts/mr_list.sh` (includes `pipeline_status`)
2. Pick one MR and check its pipeline status with `bash scripts/mr_pipeline.sh <mr_iid>`
3. If pipeline has failed, diagnose and fix the failure before addressing review items
4. Inspect unresolved review items with `bash scripts/mr_review_todo.sh <mr_iid>`
5. Evaluate each review item with the `receiving-code-review` discipline before changing code
6. Implement the fix and run the narrowest convincing verification
7. Reply in the review thread with `bash scripts/mr_reply.sh <mr_iid> <discussion_id> <body>`
8. Push the updated branch only after verification passes
9. Re-check pipeline status with `bash scripts/mr_pipeline.sh <mr_iid>` after pushing
10. Re-run `mr_review_todo.sh` until unresolved items are cleared or only intentional threads remain

## Rules

- Treat external review comments as suggestions to verify, not commands to obey blindly
- Do not claim a review item is fixed without verification evidence
- Reply in the specific discussion thread, not as an unrelated top-level MR note
- If a review item is unclear, stop and ask the user instead of guessing
- Always check pipeline status before and after pushing; do not claim an MR is ready if its pipeline has not passed
- Keep this skill focused on manual follow-up; polling and scheduling are out of scope

## Scripts

```bash
bash .project/skills/gitlab-mr-review-followup-agent/scripts/mr_list.sh
bash .project/skills/gitlab-mr-review-followup-agent/scripts/mr_pipeline.sh 413
bash .project/skills/gitlab-mr-review-followup-agent/scripts/mr_review_todo.sh 413
bash .project/skills/gitlab-mr-review-followup-agent/scripts/mr_reply.sh 413 f636f6105d9b836b99c73c656859bc80653032c3 "Fixed in latest push. Re-ran focused verification."
```

## Output Shape

- `mr_list.sh` prints JSON array of opened MRs related to the current user, with computed `roles` and `pipeline_status`
- `mr_pipeline.sh` prints a JSON object with `pipeline` details (id, status, ref, sha, web_url) and `mergeable` flag
- `mr_review_todo.sh` prints a JSON object with `unresolved_count` and `todos`
- `mr_reply.sh` prints the created note JSON from GitLab

## Verification Guidance

- Review summary scripts: verify with fixture-driven shell tests plus `bash -n`
- Real MR follow-up: record the exact verification command and outcome before replying that a fix is complete
