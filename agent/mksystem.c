#include "agent.h"

/* adds the repository's state to the system prompt s */
static void
gitcontext(stralloc *s)
{
  stralloc o = STRALLOC_ZERO;

  ingit = sh("git rev-parse --is-inside-work-tree 2>/dev/null", &o) &&
      !str_diff(o.s, "true");
  if (!ingit) goto out;
  if (!stralloc_cats(s, "\n\nThe working directory is in a git repository. "
      "As it was when this session began:\n"))
    nomem();
  if (sh("git branch --show-current 2>/dev/null", &o) && o.len &&
      (!stralloc_cats(s, "\nCurrent branch: ") || !stralloc_cat(s, &o)))
    nomem();
  if (sh("git status --short 2>/dev/null | head -30", &o) &&
      (!stralloc_cats(s, "\n\nStatus:\n") ||
      !stralloc_cats(s, o.len ? o.s : "(clean)")))
    nomem();
  if (sh("git log --oneline -5 2>/dev/null", &o) && o.len &&
      (!stralloc_cats(s, "\n\nRecent commits:\n") || !stralloc_cat(s, &o)))
    nomem();
out:
  stralloc_free(&o);
}

void
mksystem(void)
{
  struct utsname u;
  char cwd[PATH_MAX];
  stralloc md = STRALLOC_ZERO;

  if (!stralloc_copys(&sys, "You are kraft, a coding agent running in a "
      "terminal")) nomem();
  if (uname(&u) == 0) {
    if (!stralloc_cats(&sys, " on ") || !stralloc_cats(&sys, u.sysname) ||
        !stralloc_cats(&sys, " ") || !stralloc_cats(&sys, u.release))
      nomem();
  }
  if (!stralloc_cats(&sys, ". You help the user with software engineering "
      "tasks by reading and changing files and running commands"))
    nomem();
  if (getcwd(cwd, sizeof cwd)) {
    if (!stralloc_cats(&sys, " in the working directory ") ||
        !stralloc_cats(&sys, cwd))
      nomem();
  }
  if (!stralloc_cats(&sys, ".\n\n"
      "Look before you act: read files before editing them, and prefer "
      "edit_file to rewriting whole files. Run commands to build and test "
      "your changes when that helps. The user approves each edit and "
      "command and may decline; if they do, ask what they want instead. "
      "This is a plain terminal, so keep replies short and avoid Markdown "
      "tables. When a task is done, say briefly what you changed."))
    nomem();
  memory(&sys);
  stralloc_free(&md);
  gitcontext(&sys);
  if (!stralloc_0(&sys)) nomem();
}
