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
    0, tool_read },
  { "write_file",
    "Create a file, or replace a file's whole contents. Prefer edit_file "
    "for changing part of an existing file.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"File to write\"},"
    "\"content\":{\"type\":\"string\",\"description\":\"The new contents\"}},"
    "\"required\":[\"path\",\"content\"]}",
    1, tool_write },
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
    1, tool_edit },
  { "bash",
    "Run a command with /bin/sh in the working directory and return its "
    "combined stdout and stderr. Stdin is /dev/null, so the command must "
    "not wait for input. The default timeout is 120 seconds, the most 600.",
    "{\"type\":\"object\",\"properties\":{"
    "\"command\":{\"type\":\"string\",\"description\":\"The command\"},"
    "\"timeout\":{\"type\":\"integer\",\"description\":\"Seconds\"}},"
    "\"required\":[\"command\"]}",
    1, tool_bash },
  { "find_files",
    "Find files by name under a directory, recursively. pattern is a "
    "shell glob matched against each file name (such as *.c), or against "
    "the path from the directory if it contains a slash.",
    "{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\",\"description\":\"Glob, e.g. *.h\"},"
    "\"path\":{\"type\":\"string\",\"description\":\"Directory; default .\"}},"
    "\"required\":[\"pattern\"]}",
    0, tool_find },
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
    0, tool_grep },
  { "list_dir",
    "List a directory. Subdirectories are shown with a trailing slash.",
    "{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\",\"description\":\"Directory; default .\"}},"
    "\"required\":[]}",
    0, tool_ls },
  { 0, 0, 0, 0, 0 }
};
