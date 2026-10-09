#!/bin/bash
# The Editor and another program (rpgsync...) change the same game at once:
# different entries merge without a question, the same entry is asked.
#   merge_test.sh EDITOR TOOL GAME SCRIPTS_DIR
# TOOL is editor_data_merge_test, which edits and reads the files.
set -u
EDITOR="$1"; TOOL="$2"; GAME="$3"; SCRIPTS="$4"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0
fail() { echo "FAILED: $*"; failures=$((failures + 1)); }

# run SCRIPT, then the edits of the other program at their times
run() {
	rm -rf "$WORK/game" && cp -R "$GAME" "$WORK/game"
	QT_QPA_PLATFORM=offscreen "$EDITOR" --verbose --script "$SCRIPTS/$1" "$WORK/game" > "$WORK/log" 2>&1 &
	local editor=$!
	shift
	"$@"
	wait $editor || fail "the Editor ended with $?"
	grep -q "done, 0 failure(s)" "$WORK/log" || fail "$(grep -E 'failed' "$WORK/log" | head -3)"
}

other_edits() {
	sleep 2.5
	"$TOOL" rename-common-event "$WORK/game/RPG_RT.ldb" 2 Theirs || fail "cannot edit RPG_RT.ldb"
	sleep 5
	"$TOOL" rename-event "$WORK/game/Map0020.lmu" 2 Theirs || fail "cannot edit Map0020.lmu"
}
run merge.txt other_edits
grep -q "asking about a change on disk" "$WORK/log" && fail "merge: the Editor asked"
grep -q "database merged" "$WORK/log" || fail "merge: the database was not merged"
grep -q "map 20 merged" "$WORK/log" || fail "merge: the map was not merged"
[ "$("$TOOL" common-event "$WORK/game/RPG_RT.ldb" 1)" = Mine ] || fail "merge: the Editor's common event is lost"
[ "$("$TOOL" common-event "$WORK/game/RPG_RT.ldb" 2)" = Theirs ] || fail "merge: the other common event is lost"
[ "$("$TOOL" event "$WORK/game/Map0020.lmu" 2)" = Theirs ] || fail "merge: the other map event is lost"
[ "$("$TOOL" tile "$WORK/game/Map0020.lmu" 63)" != "$("$TOOL" tile "$GAME/Map0020.lmu" 63)" ] || fail "merge: the Editor's tile is lost"

same_edit() {
	sleep 2.5
	"$TOOL" rename-common-event "$WORK/game/RPG_RT.ldb" 1 Theirs || fail "cannot edit RPG_RT.ldb"
	sleep 2
}
run conflict.txt same_edit
grep -q "asking about a change on disk: The database Common Event 1" "$WORK/log" || fail "conflict: not asked"
[ "$("$TOOL" common-event "$WORK/game/RPG_RT.ldb" 1)" = Theirs ] || fail "conflict: Cancel wrote the database"

[ $failures -eq 0 ] && echo "merge test: all passed"
exit $failures
