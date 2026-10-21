#include "agent.h"

/*
 * the tools for the whole session (changing them would spoil the
 * cache), and a sub-agent's: no sub-agents of its own, no user to ask,
 * no checklist.  run after mksystem(), as the sub-agent's system
 * prompt is built on the main one.
 */
void
settools(void)
{
  size_t i, r;

  for (i = 0; tool_all[i].name && i < sizeof tools / sizeof tools[0] - 4; ++i)
    tools[i] = tool_all[i];
  tools[i++] = tasktool;
  if (access(UCTAGS, X_OK) == 0) tools[i++] = tool_symbol;
  for (r = 0; r < nmcptools && i < sizeof tools / sizeof tools[0] - 3; ++r)
    tools[i++] = mcptools[r];
  if (nmcpres) {
    /* what there is to read goes in the description */
    if (!stralloc_copys(&tmp, "Read a resource that an MCP server offers: "
        "documents, records, files and the like, as text. Some of them:\n"))
      nomem();
    for (r = 0; r < nmcpres && r < 50; ++r)
      if (!stralloc_cats(&tmp, "server ") ||
          !stralloc_cats(&tmp, mcps[mcpres[r].srv - 1].name) ||
          !stralloc_cats(&tmp, ", uri ") || !stralloc_cats(&tmp, mcpres[r].uri) ||
          (*mcpres[r].name && (!stralloc_cats(&tmp, ": ") ||
          !stralloc_cats(&tmp, mcpres[r].name))) || !stralloc_cats(&tmp, "\n"))
        nomem();
    if (nmcpres > 50 && !stralloc_cats(&tmp, "and more.\n")) nomem();
    if (!stralloc_0(&tmp)) nomem();
    restool.desc = sdup(tmp.s);
    tools[i++] = restool;
  }
  if (websearch) {
    if (pdfok)
      tools[i++] = searchtool;
    else
      kwarn("-w needs the Anthropic API at api.anthropic.com; ignored", 0, 0);
  }
  tools[i].name = 0;
  for (i = r = 0; tools[i].name; ++i)
    if (str_diff(tools[i].name, "task") && str_diff(tools[i].name, "ask_user") &&
        str_diff(tools[i].name, "todo"))
      subtools[r++] = tools[i];
  subtools[r].name = 0;
  if (!stralloc_copy(&subsys, &sys)) nomem();
  subsys.len = str_len(subsys.s);
  if (!stralloc_cats(&subsys, "\n\nYou are working as a sub-agent: the main "
      "agent has given you one task. Work on it with the tools, then reply "
      "with a concise but complete report of what you found or did. The "
      "report is all the main agent will see, so include the file names, "
      "line numbers and facts it will need. You cannot ask the user "
      "anything; if the brief is unclear, make sensible assumptions and "
      "say what they were.") || !stralloc_0(&subsys))
    nomem();
}
