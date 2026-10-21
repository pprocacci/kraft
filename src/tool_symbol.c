#include "tool.h"

/* the symbols tool; kept out of tool_all, as it needs Universal Ctags */
const struct tool tool_symbol = {
  "symbols",
  "Find where things are defined in the code, without reading whole "
  "files: functions, types, macros, variables and so on, with their "
  "file, line, kind and signature. Give a name (exact, or a glob such "
  "as parse_*) to find its definition; a kind (function, struct, "
  "macro, ...) to narrow it; and a path to look only there. A file "
  "path with no name gives that file's outline. Use it to find your "
  "way around a large codebase before reading the parts that matter.",
  "{\"type\":\"object\",\"properties\":{"
  "\"name\":{\"type\":\"string\",\"description\":\"Name or glob\"},"
  "\"kind\":{\"type\":\"string\",\"description\":\"function, struct, ...\"},"
  "\"path\":{\"type\":\"string\","
  "\"description\":\"File or directory; default .\"}},"
  "\"required\":[]}",
  0, tool_symbols, 0, 0, 0
};
