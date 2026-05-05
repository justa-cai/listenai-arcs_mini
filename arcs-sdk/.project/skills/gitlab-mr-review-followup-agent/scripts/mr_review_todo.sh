#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <mr_iid>" >&2
  exit 1
fi

project_id="${PROJECT_ID:-2450}"
mr_iid="$1"

if [ -n "${MR_DISCUSSIONS_JSON:-}" ]; then
  discussions_json="$MR_DISCUSSIONS_JSON"
else
  discussions_json="$(glab api "projects/$project_id/merge_requests/$mr_iid/discussions?per_page=100")"
fi

printf '%s\n' "$discussions_json" | jq --argjson mr_iid "$mr_iid" '
  def note_location:
    (.position // {}) as $pos
    | if ($pos | length) == 0 then null
      else
        (($pos.new_path // $pos.old_path) // "") as $path
        | (($pos.new_line // $pos.old_line)) as $line
        | if ($path == "") and ($line == null) then null
          else
            $path + (if $line == null then "" else ":" + ($line | tostring) end)
          end
      end;

  def open_notes_array:
    [
      .notes[]?
      | select((.resolvable == true) and (.resolved == false) and (.system != true))
    ];

  {
    mr_iid: $mr_iid,
    unresolved_count: [
      .[]
      | select(.individual_note != true)
      | open_notes_array
      | select(length > 0)
    ] | length,
    todos: [
      .[]
      | select(.individual_note != true)
      | open_notes_array as $notes
      | select($notes | length > 0)
      | {
          discussion_id: .id,
          note_id: ($notes[0].id),
          author: ($notes[0].author.username // null),
          body: ($notes[0].body // ""),
          location: (($notes[0] | note_location) // "general"),
          unresolved_notes: ($notes | length)
        }
    ]
  }
'
