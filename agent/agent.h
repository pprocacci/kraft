#ifndef AGENT_H
#define AGENT_H
/*
 * The kraft program, apart from the library: its shared state
 * (defined in state.c) and its functions, one to a file.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "alloc.h"
#include "anth.h"
#include "api.h"
#include "buffer.h"
#include "byte.h"
#include "cfg.h"
#include "conv.h"
#include "file.h"
#include "http.h"
#include "job.h"
#include "json.h"
#include "mcp.h"
#include "md.h"
#include "media.h"
#include "oai.h"
#include "path.h"
#include "perm.h"
#include "proc.h"
#include "scan.h"
#include "sse.h"
#include "str.h"
#include "strerr.h"
#include "stralloc.h"
#include "tcp.h"
#include "tls.h"
#include "tool.h"
#include "ui.h"
#include "undo.h"
#include "url.h"
#ifndef KRAFT_VERSION
#define KRAFT_VERSION "unknown"
#endif
#define FATAL "kraft: fatal: "
#define USAGE "kraft: usage: kraft [-jprtwy] [-a anthropic|openai] [-c tokens] " \
    "[-e effort] [-m model] " \
    "[-n max_tokens] [-P ask|rules|auto|yes] [-u url] [task ...]"
#define MAXSTEPS 100  /* model requests per task */
#define CONNECTSECS 15  /* to give up connecting to the server */
/* permission modes: when edits and commands need the user's approval */
#define MODE_ASK 0    /* always */
#define MODE_RULES 1  /* unless perm_auto() allows them */
#define MODE_AUTO 2   /* unless perm_auto() or, if it is unsure, check() does */
#define MODE_YES 3    /* never */
#define BOLD "1"
#define DIM "2"
#define RED "31"
#define GREEN "32"
#define YELLOW "33"
#define CYAN "36"
#define TOOLC "1;36"
#define RETRIES 5  /* more tries for a request that failed for now */
/*
 * Images and PDFs.  read_file on one, or @path in the user's words,
 * attaches it to the conversation for the model to see.  PDFs go only
 * to the Anthropic API itself; elsewhere read_file reads their text.
 */
#define MAXIMAGE (3750 * 1024)   /* 5 MB once base64-encoded, the API's limit */
#define MAXPDF (16 * 1024 * 1024)
/*
 * Sub-agents.  The task tool hands a brief to a fresh conversation
 * with the same model and nearly the same tools; it works through it
 * and only its final report comes back, so what it read does not
 * fill the main conversation.
 */
#define SUBSTEPS 40

    /* -w: offer Anthropic's server-side web search */

/* a permission rule from a config file; see setting() */
struct rule {
  int allow;
  const char *tool;
  const char *pat;
  const char *from;  /* the file it came from */
};

/* state.c */
extern const char *const modes[];
extern const struct api apis[2];  /* anthropic, openai */
extern const struct api *api;
extern const char *model, *base;
extern const char *checkmodel;
extern const char *effort, *effortnow;
extern unsigned long long cost;
extern int costgap, ctxset, script;
extern unsigned long max;
extern int showthink, mode, status;
extern volatile sig_atomic_t intr;
extern volatile sig_atomic_t curfd;
extern struct tool tools[256];
extern int allowed[256];
extern int websearch;
extern stralloc ruledeny;
extern const struct tool searchtool;
extern char last;
extern struct url url;
extern struct conv conv;
extern struct http http;
extern struct sse sse;
extern struct reply reply;
extern stralloc host, path, hdrs, sys, body, event, data, line, tmp, tlswhy;
extern struct conv cconv;
extern stralloc rwhy, cwhy;
extern stralloc mwhy;
extern const struct usage nousage;
extern struct usage total;
extern struct usage lastmain;
extern unsigned long requests;
extern unsigned long ctxwin;
extern unsigned long pending;
extern int overflow;
extern int again;
extern unsigned long waitsecs;
extern stralloc pfx, ghdrs;
extern int color;
extern int ingit;
extern const char commitprompt[];
extern struct md md;
extern stralloc mdout;
extern struct timespec asked;
extern stralloc sessfn;
extern stralloc cwdsa;
extern int savewarned;
extern stralloc undodir;
extern unsigned long undoseq;
extern unsigned long taskno;
extern const struct tool notools[];
extern const char checksys[];
extern int pdfok;
extern struct tool subtools[256];
extern stralloc subsys;
extern const struct tool tasktool;
extern struct rule rules[256];
extern unsigned long nrules;
extern const char *cfgfile;
extern const char *cfgapi;
extern stralloc projcfg;
extern stralloc word;

/* one file each */
void announce(const char *prefix, const struct block *b, const char *in, size_t n);
int approve(const struct tool *t, const char *in, size_t n);
int attach(struct msg *m, const stralloc *id, const char *path, stralloc *why);
int attempt(struct conv *c, const char *sysp, const struct tool *tools, const char *mdl, unsigned long mx, int show);
int check(const struct tool *t, const char *in, size_t n);
int compact(int carry);
const char * env(const char *name, const char *dflt);
unsigned long estimate(void);
int exchange(struct conv *c, const char *sysp, const struct tool *tools, const char *mdl, unsigned long mx, int show);
void flush(void);
int hastools(const struct msg *m);
int hits(const char *pat, const char *s, int parts);
int isedit(const char *name);
void showedit(const struct tool *t, const char *in, size_t n);
void kwarn(const char *a, const char *b, const char *c);
void kwarnsys(const char *a, const char *b);
void lines(const char *p, const char *s, size_t n, unsigned long max);
void loadconfigs(void);
void mksystem(void);
void nl(void);
void nomem(void) __dead2;
void out(const char *s, size_t n);
void outs(const char *s);
int pickmode(const char *s);
int pickeffort(const char *s);
int pickeffort(const char *s);
int plain(const char *s);
void raw(const char *s, size_t n);
void relpath(const char *p, stralloc *out);
int resume(void);
int ruled(const struct tool *t, const char *in, size_t n, const struct rule **why);
int runtools(struct conv *c, const struct tool *tl, int cut, int sub);
void save(void);
int saverule(const struct tool *t, const char *in, size_t n);
char * sdup(const char *s);
int sessdir(stralloc *d);
int setting(void *arg, const char *k, const char *v, unsigned long line);
void setundo(void);
void sgr(const char *code);
int sh(const char *cmd, stralloc *out);
void spin(int fd);
void styled(const char *code, const char *s);
int subagent(const char *in, size_t n, stralloc *out);
void task(const char *s, size_t n);
void trust(const char *fn, const char *hash);
int undo(void);
void work(const char *s, size_t n);

char **options(int argc, char **argv, int *resumeit);
void setconnection(void);
void settools(void);
int command(const char *line);
void repl(void);
void findwindow(void);
void newsession(void);
void memory(stralloc *s);
int custom(const char *line);
void customs(void);
int price(const char *m, const struct usage *u, unsigned long long *cost);
void catcost(stralloc *s);
void scriptout(int json);

/* MCP servers, from the config, and their tools */
#define MAXMCP 16
#define MAXMCPTOOLS 200
extern struct mcp mcps[MAXMCP];
extern unsigned long nmcps;
extern struct tool mcptools[MAXMCPTOOLS];
extern unsigned long nmcptools;
void mcpstart(void);
void mcpstop(void);
int mcpcall(const struct tool *t, const char *in, size_t n, stralloc *out);
void mcplist(void);

#endif
