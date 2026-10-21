#include "tool.h"

const struct tool tool_all[] = {
  { "read_file",
    "Read a text file. Returns its lines numbered from 1, at most 2000 "
    "lines at a time; use offset and limit to page through long files. "
    "Read a file before editing it.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"File to read\"},"
    "\"offset\":{\"type\":\"integer\",\"description\":\"First line, from 1\"},"
    "\"limit\":{\"type\":\"integer\",\"description\":\"Number of lines\"}},"
    "\"required\":[\"path\"]}",
    0, tool_read, 0 },
  { "write_file",
    "Create a file, or replace a file's whole contents. Prefer edit_file "
    "for changing part of an existing file.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"File to write\"},"
    "\"content\":{\"type\":\"string\",\"description\":\"The new contents\"}},"
    "\"required\":[\"path\",\"content\"]}",
    1, tool_write, 0 },
  { "edit_file",
    "Replace exact text in a file. old_string must match the file "
    "exactly, including whitespace, and must occur once unless "
    "replace_all is true; include enough surrounding lines to make it "
    "unique. Line-number prefixes from read_file are not part of the file.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"File to edit\"},"
    "\"old_string\":{\"type\":\"string\",\"description\":\"Text to replace\"},"
    "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement\"},"
    "\"replace_all\":{\"type\":\"boolean\","
    "\"description\":\"Replace every occurrence\"}},"
    "\"required\":[\"path\",\"old_string\",\"new_string\"]}",
    1, tool_edit, 0 },
  { "multi_edit",
    "Make several exact replacements in one file at once. The edits are "
    "applied in order, each to the result of the one before, with the "
    "same rules as edit_file; if any fails, the file is left unchanged. "
    "Prefer it to several edit_file calls on the same file.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"File to edit\"},"
    "\"edits\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
    "\"properties\":{"
    "\"old_string\":{\"type\":\"string\"},"
    "\"new_string\":{\"type\":\"string\"},"
    "\"replace_all\":{\"type\":\"boolean\"}},"
    "\"required\":[\"old_string\",\"new_string\"]}}},"
    "\"required\":[\"path\",\"edits\"]}",
    1, tool_multiedit, 0 },
  { "bash",
    "Run a command with /bin/sh in the working directory and return its "
    "combined stdout and stderr. Stdin is /dev/null, so the command must "
    "not wait for input. The default timeout is 120 seconds, the most 600. "
    "For something long-running, such as a server or a slow build, set "
    "background: it starts, and you read its output with bash_output.",
    "{\"type\":\"object\",\"properties\":{"
    "\"command\":{\"type\":\"string\",\"description\":\"The command\"},"
    "\"timeout\":{\"type\":\"integer\",\"description\":\"Seconds\"},"
    "\"background\":{\"type\":\"boolean\","
    "\"description\":\"Run it in the background\"}},"
    "\"required\":[\"command\"]}",
    1, tool_bash, 0 },
  { "bash_output",
    "Read new output from a background command since the last read, and "
    "whether it is still running. wait is how many seconds to wait for "
    "output if there is none yet (at most 120). With no id, list the "
    "background commands.",
    "{\"type\":\"object\",\"properties\":{"
    "\"id\":{\"type\":\"integer\",\"description\":\"The job number\"},"
    "\"wait\":{\"type\":\"integer\",\"description\":\"Seconds to wait\"}},"
    "\"required\":[]}",
    0, tool_jobout, 0 },
  { "bash_kill",
    "Stop a background command.",
    "{\"type\":\"object\",\"properties\":{"
    "\"id\":{\"type\":\"integer\",\"description\":\"The job number\"}},"
    "\"required\":[\"id\"]}",
    0, tool_jobkill, 0 },
  { "find_files",
    "Find files by name under a directory, recursively. pattern is a "
    "shell glob matched against each file name (such as *.c), or against "
    "the path from the directory if it contains a slash.",
    "{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\",\"description\":\"Glob, e.g. *.h\"},"
    "\"path\":{\"type\":\"string\",\"description\":\"Directory; default .\"}},"
    "\"required\":[\"pattern\"]}",
    0, tool_find, 0 },
  { "grep",
    "Search file contents for an extended regular expression, "
    "recursively. Returns matching lines as file:line:text. Binary files "
    "and .git are skipped.",
    "{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\",\"description\":\"Regular expression\"},"
    "\"path\":{\"type\":\"string\","
    "\"description\":\"File or directory; default .\"},"
    "\"glob\":{\"type\":\"string\","
    "\"description\":\"Only search files whose names match, e.g. *.c\"}},"
    "\"required\":[\"pattern\"]}",
    0, tool_grep, 0 },
  { "list_dir",
    "List a directory. Subdirectories are shown with a trailing slash.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"Directory; default .\"}},"
    "\"required\":[]}",
    0, tool_ls, 0 },
  { "web_fetch",
    "Fetch a web page or file over http or https and return its text; "
    "HTML is turned into plain text. Use it for documentation, release "
    "notes, bug reports and the like. Only text is returned, at most "
    "max_length bytes (default 20000).",
    "{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"The http(s) URL\"},"
    "\"max_length\":{\"type\":\"integer\","
    "\"description\":\"Most bytes of text to return\"}},"
    "\"required\":[\"url\"]}",
    1, tool_fetch, 0 },
  { "todo",
    "Keep a checklist for a task with several steps, which the user "
    "sees. Send the whole list each time it changes; mark one item "
    "in_progress while you work on it and completed as soon as it is "
    "done. Skip it for simple tasks.",
    "{\"type\":\"object\",\"properties\":{"
    "\"todos\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
    "\"properties\":{"
    "\"content\":{\"type\":\"string\",\"description\":\"The step\"},"
    "\"status\":{\"type\":\"string\","
    "\"enum\":[\"pending\",\"in_progress\",\"completed\"]}},"
    "\"required\":[\"content\",\"status\"]}}},"
    "\"required\":[\"todos\"]}",
    0, tool_todo, 0 },
  { "ask_user",
    "Ask the user a question and wait for the answer, when you need a "
    "decision or a fact only they have, rather than guessing. Give "
    "options when there are a few likely answers; they may still answer "
    "in their own words.",
    "{\"type\":\"object\",\"properties\":{"
    "\"question\":{\"type\":\"string\",\"description\":\"The question\"},"
    "\"options\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},"
    "\"description\":\"Likely answers to choose from\"}},"
    "\"required\":[\"question\"]}",
    0, tool_ask, 0 },
  { 0, 0, 0, 0, 0, 0 }
};
