#!/bin/sh
# kraft from end to end, against the scripted model server t/fakeapi:
# run from the top of the source tree, after make.

top=$(pwd)
K=$top/kraft
T=$(mktemp -d "${TMPDIR:-/tmp}/kraft-t.XXXXXX") || exit 111
P=$T/home/proj
fails=0
fpid=
extra=

trap '[ -n "$fpid" ] && kill $fpid 2>/dev/null; rm -rf "$T"' EXIT

# a fresh home and project, and the script for the server from stdin
setup() {
	name=$1
	rm -rf "$T/home" "$T/log" "$T/script"
	mkdir -p "$T/home/.kraft" "$P" "$T/log"
	cat > "$T/script"
}

# kraft with these arguments (and stdin), against the script
run() {
	rm -f "$T/port"
	"$top/t/fakeapi" "$T/script" "$T/log" "$T/port" &
	fpid=$!
	i=0
	while [ ! -s "$T/port" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
	(cd "$P" && env -i PATH="$PATH" HOME="$T/home" TERM=dumb NO_COLOR=1 \
	    KRAFT_API_KEY=test $extra "$K" -u "http://127.0.0.1:$(cat "$T/port")" \
	    -m fake "$@" > "$T/out" 2> "$T/err")
	status=$?
	kill $fpid 2>/dev/null
	wait $fpid 2>/dev/null
	fpid=
}

fail() {
	echo "FAIL $name: $*"
	sed 's/^/  err: /' "$T/err" | head -20
	sed 's/^/  out: /' "$T/out" | head -10
	fails=$((fails + 1))
}

# expectations
status() { [ "$status" = "$1" ] || fail "exit status $status, not $1"; }
out() { grep -q -- "$1" "$T/out" || fail "stdout lacks: $1"; }
err() { grep -q -- "$1" "$T/err" || fail "stderr lacks: $1"; }
noerr() { ! grep -q -- "$1" "$T/err" || fail "stderr has: $1"; }
req() { grep -q -- "$2" "$T/log/$1" 2>/dev/null || fail "request $1 lacks: $2"; }
reqs() {
	n=$(ls "$T/log" | wc -l | tr -d ' ')
	[ "$n" = "$1" ] || fail "$n requests, not $1"
}
file() { [ -f "$P/$1" ] && grep -q -- "$2" "$P/$1" || fail "$1 lacks: $2"; }
nofile() { [ ! -e "$P/$1" ] || fail "$1 exists"; }

for api in anthropic openai; do

setup "$api: a plain answer" <<EOF
reply
think let me see
text Hello\nthere, world.
EOF
run -a $api -p "say hello"
status 0
out "Hello"
out "there, world."
req 1 "say hello"
reqs 1

setup "$api: a tool, then the answer" <<EOF
reply
text I will write it.
tool write_file {"path":"a.txt","content":"one\\ntwo\\n"}
reply
text Written.
EOF
run -a $api -P yes -p "write a.txt"
status 0
out "Written."
file a.txt two
req 2 "toolu_1\|call_1"
req 2 "a.txt"
reqs 2

setup "$api: two calls in one reply" <<EOF
reply
tool bash {"command":"echo first-out"}
tool bash {"command":"echo second-out >&2; exit 3"}
reply
text Ran both.
EOF
run -a $api -P yes -p "run them"
status 0
req 2 "first-out"
req 2 "second-out"
req 2 "exit status 3\|exit 3\|status 3"

done

setup "an unknown tool and bad input" <<EOF
reply
tool nonesuch {}
tool read_file not json
reply
text Sorry.
EOF
run -P yes -p "go"
status 0
req 2 "no tool by that name"
req 2 "not a valid JSON object"

setup "no terminal to ask on: declined" <<EOF
reply
tool bash {"command":"touch made"}
reply
text OK.
EOF
run -P ask -p "make it" < /dev/null
status 0
nofile made
req 2 "declined"
err "no terminal to ask on"

setup "a deny rule holds even in yes mode" <<EOF
reply
tool bash {"command":"git push origin main"}
reply
text OK.
EOF
echo 'deny bash git push*' > "$T/home/.kraft/config"
run -P yes -p "push"
status 0
req 2 "refused by the user's rule"

setup "rules mode: harmless commands allowed, others asked" <<EOF
reply
tool bash {"command":"ls -l"}
tool bash {"command":"touch made"}
reply
text OK.
EOF
run -P rules -p "go" < /dev/null
status 0
nofile made
req 2 "total"
req 2 "declined"

setup "an edit outside the project is not auto-allowed" <<EOF
reply
tool write_file {"path":"../outside.txt","content":"x"}
reply
text OK.
EOF
run -P rules -p "go" < /dev/null
[ ! -e "$T/home/outside.txt" ] || fail "wrote outside the project"
req 2 "declined"

setup "retries after a 529" <<EOF
status 529
reply
text Made it.
EOF
run -p "try"
status 0
out "Made it."
reqs 2

setup "gives up on a 400" <<EOF
status 400
EOF
run -p "try"
[ "$status" != 0 ] || fail "exit status 0 after a 400"
reqs 1

setup "a reply cut off mid-call" <<EOF
reply
tool bash {"command":"touch made"}
stop max_tokens
reply
text OK.
EOF
run -P yes -p "go"
nofile made
err "not run: the reply was cut off"
err "raise it with -n"
reqs 1

setup "parallel: reads all at once, then the rest in order" <<EOF
reply
tool read_file {"path":"r1.txt"}
tool list_dir {"path":"."}
tool grep {"pattern":"needle"}
tool write_file {"path":"r1.txt","content":"changed"}
tool read_file {"path":"r1.txt"}
reply
text OK.
EOF
echo "first needle" > "$P/r1.txt"
run -P yes -p "go"
status 0
req 2 "first needle"
req 2 "r1.txt:1"
req 2 "changed"
# the three reads are announced before any of their results
awk '/^\* /{c++} /^  [|!] /{if (c < 3) bad=1; exit} END{exit bad}' "$T/err" ||
    fail "the reads did not start together"

setup "parallel: sub-agents at once, their costs counted" <<EOF
reply
tool task {"description":"one","prompt":"write x.txt"}
tool task {"description":"two","prompt":"write y.txt"}
reply
tool write_file {"path":"x.txt","content":"x"}
reply
tool write_file {"path":"y.txt","content":"y"}
reply
text Report from a sub-agent.
reply
text Report from a sub-agent.
reply
text Both done.
EOF
printf 'go\n/undo\n' | run -P yes
status 0
reqs 6
req 6 "Report from a sub-agent.*Report from a sub-agent"
nofile x.txt
nofile y.txt
out "removed .*x.txt"
out "removed .*y.txt"

setup "parallel: -j counts the sub-agents' requests" <<EOF
reply
tool task {"description":"one","prompt":"look"}
tool task {"description":"two","prompt":"look"}
reply
text Seen.
reply
text Seen.
reply
text Done.
EOF
run -P yes -j "go"
status 0
out '"requests":4'
out '"output_tokens":80'

setup "hooks: before blocks, after reports, done runs" <<EOF
reply
tool bash {"command":"rm -f keep"}
tool bash {"command":"echo fine"}
tool write_file {"path":"c.txt","content":"x"}
reply
text OK.
EOF
cat > "$T/home/.kraft/config" <<'CFG'
hook before bash case "$KRAFT_COMMAND" in *rm*) echo no removing; exit 1;; esac
hook after edit echo "checked $KRAFT_PATH, failed=$KRAFT_FAILED"
hook after mcp__* echo never
hook done echo "task over, failed=$KRAFT_FAILED"
CFG
touch "$P/keep"
run -P yes -p "go"
status 0

[ -f "$P/keep" ] || fail "the blocked rm ran"
req 2 "blocked by the user's hook"
req 2 "no removing"
req 2 "fine"
req 2 "checked c.txt, failed=0"
! grep -q never "$T/log/2" || fail "a hook for another tool ran"
err "task over, failed=0"

setup "-j: the outcome as JSON" <<EOF
reply
tool read_file {"path":"nope.txt"}
reply
text All done.
EOF
run -P yes -j "go"
status 0
out '"result":"All done."'
out '"requests":2'
out '"error":false'

setup "memory: KRAFT.md goes in the system prompt" <<EOF
reply
text OK.
EOF
echo 'Always answer in limericks.' > "$P/KRAFT.md"
echo 'The user is called Kim.' > "$T/home/.kraft/KRAFT.md"
run -p "hi"
req 1 "limericks"
req 1 "called Kim"

setup "the REPL: a task, a custom command, /undo" <<EOF
reply
tool write_file {"path":"b.txt","content":"new"}
reply
text Changed.
reply
text Reviewed.
EOF
echo old > "$P/b.txt"
mkdir -p "$T/home/.kraft/commands"
echo 'Review $ARGUMENTS carefully.' > "$T/home/.kraft/commands/review.md"
printf 'change b.txt\n/review the parser\n/undo\n/nonesuch\n' | run -P yes
status 0
file b.txt old
req 3 "Review the parser carefully."
out "no such command"

setup "a project config is ignored until trusted" <<EOF
reply
tool bash {"command":"touch made"}
reply
text OK.
EOF
mkdir -p "$P/.kraft"
echo 'mode yes' > "$P/.kraft/config"
run -p "go" < /dev/null
nofile made
err "until you trust it"

setup "MCP: a server's tools are offered and called" <<EOF
reply
tool mcp__toy__add_two {"a":2,"b":40}
tool mcp__toy__fail {}
reply
text 42.
EOF
echo "mcp toy $top/t/mcpd" > "$T/home/.kraft/config"
run -P yes -p "add"
status 0
req 1 "mcp__toy__echo"
req 1 '"properties":{}'
req 2 '42'
req 2 'it failed'

setup "MCP: prompts as commands, resources by @ and by tool" <<EOF
reply
text Hello, Kim!
reply
tool read_resource {"server":"toy","uri":"toy://logo"}
tool read_resource {"server":"toy","uri":"toy://nope"}
reply
text It says to be excellent.
EOF
echo "mcp toy $top/t/mcpd" > "$T/home/.kraft/config"
printf '/toy:greet Kim very cheerful\nwhat does @toy:toy://motd say?\n/toy:greet\n/mcp\n' |
    run -P yes
status 0
req 1 "Say hello to Kim in a very cheerful way."
req 1 '"read_resource"'
req 1 "toy://motd: Message of the day"
req 2 "Be excellent to each other."
req 3 "binary image/png, not shown"
req 3 "no such resource"
reqs 3
err "the prompt needs who"
out "/toy:greet who \[tone\]"
out "@toy:toy://motd"

setup "MCP: a server that will not start" <<EOF
reply
text OK.
EOF
echo "mcp broken exit 1" > "$T/home/.kraft/config"
run -p "go"
status 0
err "MCP server broken"

# the toy server on HTTP, for the next two
"$top/t/mcpd" -h "$T/mport" &
mpid=$!
i=0
while [ ! -s "$T/mport" ] && [ $i -lt 100 ]; do sleep 0.05; i=$((i + 1)); done
murl="http://127.0.0.1:$(cat "$T/mport")/mcp"

setup "MCP over HTTP, with a token from the environment" <<EOF
reply
tool mcp__web__echo {"text":"over the wire"}
tool mcp__web__add_two {"a":1,"b":2}
reply
text Done.
EOF
printf 'mcp web %s\nmcp_header web Authorization: Bearer ${MCP_TOKEN}\n' \
    "$murl" > "$T/home/.kraft/config"
extra="MCP_TOKEN=sesame"
run -P yes -p "go"
extra=
status 0
noerr "MCP server"
req 1 "mcp__web__fail"
req 2 "over the wire"
req 2 '"3'

setup "MCP over HTTP without credentials" <<EOF
reply
text OK.
EOF
echo "mcp web $murl" > "$T/home/.kraft/config"
run -p "go"
status 0
err "MCP server web"
err "HTTP 401"
kill $mpid 2>/dev/null

if [ -x /usr/local/bin/uctags ]; then
setup "symbols: definitions from ctags" <<EOF
reply
tool symbols {"name":"frob*"}
reply
text Found.
EOF
printf 'int\nfrobnicate(int x)\n{\n  return x;\n}\n' > "$P/f.c"
run -p "find frob"
req 1 '"symbols"'
req 2 'f.c:2'
req 2 'frobnicate'
fi

if [ $fails -gt 0 ]; then
	echo "agent: $fails failed"
	exit 1
fi
echo "agent: ok"
