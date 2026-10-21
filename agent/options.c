#include "agent.h"

/*
 * settles the settings: the defaults, then the config files, then the
 * environment, then the flags.  sets *resumeit for -r, and returns
 * what is left of argv: the words of a task, if any.
 */
char **
options(int argc, char **argv, int *resumeit)
{
  const char *aname, *key;
  size_t i;
  int opt;

  loadconfigs();
  aname = env("KRAFT_API", cfgapi ? cfgapi : "anthropic");
  model = env("KRAFT_MODEL", model);
  base = env("KRAFT_URL", base ? base : "http://127.0.0.1:1234");
  checkmodel = env("KRAFT_CHECK_MODEL", checkmodel);
  if (!pickmode(env("KRAFT_MODE", modes[mode])))
    strerr_die(100, FATAL, "KRAFT_MODE must be ask, rules, auto or yes", 0);
  if ((key = env("KRAFT_EFFORT", 0)) && !pickeffort(key))
    strerr_die(100, FATAL, "KRAFT_EFFORT must be low, medium, high, xhigh, "
        "max or default", 0);
  if ((key = env("KRAFT_CONTEXT", 0)) &&
      (scan_ulong(key, &ctxwin) != str_len(key) || !ctxwin))
    strerr_die(100, FATAL, "KRAFT_CONTEXT must be a number of tokens", 0);
  *resumeit = 0;
  while ((opt = getopt(argc, argv, "a:c:e:jm:n:pP:rtu:wy")) != -1)
    switch (opt) {
    case 'a': aname = optarg; break;
    case 'c':
      if (!*optarg || scan_ulong(optarg, &ctxwin) != str_len(optarg) ||
          !ctxwin)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 'e': if (!pickeffort(optarg)) strerr_die(100, USAGE, 0, 0); break;
    case 'm': model = optarg; break;
    case 'n':
      if (!*optarg || scan_ulong(optarg, &max) != str_len(optarg) || !max)
        strerr_die(100, USAGE, 0, 0);
      break;
    case 'j': script = 2; break;
    case 'p': script = 1; break;
    case 'P': if (!pickmode(optarg)) strerr_die(100, USAGE, 0, 0); break;
    case 'r': *resumeit = 1; break;
    case 'w': websearch = 1; break;
    case 't': showthink = 1; break;
    case 'u': base = optarg; break;
    case 'y': mode = MODE_YES; break;
    default: strerr_die(100, USAGE, 0, 0);
    }

  for (i = 0; i < sizeof apis / sizeof apis[0]; ++i)
    if (!str_diff(aname, apis[i].name)) api = &apis[i];
  if (!api) strerr_die(100, FATAL, "unknown API: ", aname);
  if (!model) strerr_die(100, FATAL, "no model: use -m or set KRAFT_MODEL", 0);
  if (!checkmodel) checkmodel = model;
  ctxset = ctxwin != 0;  /* given in a config, the environment or -c */
  return argv + optind;
}
