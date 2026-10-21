#include "agent.h"

static int resource(struct msg *m, const char *p);
static void mentions(struct msg *m, const char *s, size_t n);
static void interrupted(void);

/*
 * @server:uri names an MCP server's resource: its text goes in m.
 * returns 1 if p was one of those (read or not).
 */
static int
resource(struct msg *m, const char *p)
{
  stralloc text = STRALLOC_ZERO;
  struct block *b;
  size_t c = byte_chr(p, str_len(p), ':');
  unsigned long k;

  for (k = 0; k < nmcps; ++k)
    if (str_len(mcps[k].name) == c && !byte_diff(mcps[k].name, c, p)) break;
  if (!p[c] || k == nmcps) return 0;
  if (!mcpread(p, c, p + c + 1, &text)) {
    if (!stralloc_0(&text)) nomem();
    outs("(");
    outs(text.s);
    outs(")\n");
  } else {
    if (!(b = conv_block(m, BLK_TEXT))) nomem();
    if (!stralloc_copys(&b->text, "The resource ") ||
        !stralloc_cats(&b->text, p + c + 1) ||
        !stralloc_cats(&b->text, " from MCP server ") ||
        !stralloc_cats(&b->text, mcps[k].name) ||
        !stralloc_cats(&b->text, ":\n\n") || !stralloc_cat(&b->text, &text))
      nomem();
    pending += b->text.len;
    sgr(DIM);
    outs("(attached ");
    outs(p);
    outs(")\n");
    sgr(0);
  }
  stralloc_free(&text);
  return 1;
}

/* attaches the files named @path in the user's words s to message m */
static void
mentions(struct msg *m, const char *s, size_t n)
{
  stralloc p = STRALLOC_ZERO, why = STRALLOC_ZERO, noid = STRALLOC_ZERO;
  size_t i, e;

  for (i = 0; i < n; ++i) {
    if (s[i] != '@' || (i && s[i - 1] != ' ' && s[i - 1] != '\n' &&
        s[i - 1] != '\t' && s[i - 1] != '('))
      continue;
    for (e = i + 1; e < n && s[e] != ' ' && s[e] != '\n' && s[e] != '\t'; ++e) ;
    while (e > i + 1 && (s[e - 1] == '.' || s[e - 1] == ',' ||
        s[e - 1] == ';' || s[e - 1] == ':' || s[e - 1] == ')' ||
        s[e - 1] == '!' || s[e - 1] == '?'))
      --e;
    if (e == i + 1) continue;
    if (!stralloc_copyb(&p, s + i + 1, e - i - 1) || !stralloc_0(&p)) nomem();
    if (resource(m, p.s)) {
      i = e;
      continue;
    }
    if (!stralloc_copys(&why, "")) nomem();
    if (attach(m, &noid, p.s, &why) && why.len) {
      if (!stralloc_0(&why)) nomem();
      outs("(");
      outs(why.s);
      outs(")\n");
    }
    i = e;
  }
  stralloc_free(&p);
  stralloc_free(&why);
}

/* tells the model, in the user's next turn, that it was cut short */
static void
interrupted(void)
{
  struct msg *m;
  struct block *b;

  if (!(m = conv_msg(&conv, ROLE_USER))) nomem();
  if (!(b = conv_block(m, BLK_TEXT))) nomem();
  if (!stralloc_copys(&b->text, "[The user interrupted. Stop what you were "
      "doing and wait for their next message.]"))
    nomem();
  status = 130;
}

/* carries out one task: the user's words, then model and tools in turn */
void
work(const char *s, size_t n)
{
  struct msg *m;
  struct block *b;
  int step, r, stop, retried = 0;

  intr = 0;
  ++taskno;  /* for /undo */
  if (!(m = conv_msg(&conv, ROLE_USER))) nomem();
  if (!(b = conv_block(m, BLK_TEXT))) nomem();
  if (!stralloc_copyb(&b->text, s, n)) nomem();
  pending += n;
  mentions(m, s, n);  /* @file.png and the like */

  for (step = 0; step < MAXSTEPS; ++step) {
    /* compact before the conversation outgrows three quarters of the window */
    if (ctxwin && estimate() > ctxwin / 4 * 3 && !compact(1)) return;
    r = exchange(&conv, sys.s, tools, model, max, 1);
    if (!r && overflow && !retried) {
      /* the server says it is too long after all: compact, try again */
      overflow = 0;
      retried = 1;
      if (!compact(1)) return;
      r = exchange(&conv, sys.s, tools, model, max, 1);
    }
    if (!r && overflow) {
      kwarn("the conversation is too long for the model; /clear starts "
          "afresh", 0, 0);
      overflow = 0;
      status = 100;
    }
    if (r < 0) { interrupted(); return; }
    if (!r) return;
    m = &conv.m[conv.n - 1];
    if (reply.stop == STOP_REFUSAL) kwarn("the model declined this request", 0, 0);
    if (reply.stop == STOP_PAUSE && !hastools(m)) {
      save();
      continue;  /* a server tool is still at work: send it all back */
    }
    if (!hastools(m)) {
      if (reply.stop == STOP_MAX)
        kwarn("the reply was cut off at max_tokens; raise it with -n", 0, 0);
      return;
    }
    stop = reply.stop;  /* the auto-mode check reuses reply */
    if (!runtools(&conv, tools, stop == STOP_MAX, 0)) { interrupted(); return; }
    save();
    if (stop == STOP_MAX) {
      kwarn("the reply was cut off at max_tokens; raise it with -n", 0, 0);
      return;
    }
  }
  kwarn("stopped: too many steps for one task", 0, 0);
}
