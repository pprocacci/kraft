#include "agent.h"

/* the uncommitted changes in short: "2 files changed, ..." */
static void
gitstat(stralloc *out)
{
  size_t i;

  if (!ingit || !sh("git diff --shortstat HEAD 2>/dev/null", out)) {
    if (!stralloc_copys(out, "") || !stralloc_0(out)) nomem();
    return;
  }
  for (i = 0; i < out->len && out->s[i] == ' '; ++i) ;
  byte_copy(out->s, out->len - i + 1, out->s + i);
  out->len -= i;
}

void
task(const char *s, size_t n)
{
  static stralloc before, after, said;

  gitstat(&before);
  work(s, n);
  save();
  if (nhooks) {
    /* a notification, say, or the tests */
    hook(HOOK_DONE, 0, 0, 0, 0, status != 0, &said);
    if (said.len) {
      nl();
      lines("  hook: ", said.s, said.len, 20);
      flush();
    }
  }
  /* what the task left uncommitted, if it changed that */
  gitstat(&after);
  if (after.len && str_diff(before.s, after.s)) {
    nl();
    sgr(DIM);
    outs("  git: ");
    outs(after.s);
    outs(" (uncommitted; /diff to see)");
    sgr(0);
    outs("\n");
    flush();
  }
}
