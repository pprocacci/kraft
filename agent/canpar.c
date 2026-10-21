#include "agent.h"

/* the tools that may run side by side: they change nothing */
static const char *const free_[] = {
  "read_file", "grep", "find_files", "list_dir", "symbols", "task", 0
};

/* is read_file's file an image or a PDF, which attach() deals with? */
static int
media(const char *in, size_t n)
{
  static stralloc p;
  char b[16];
  ssize_t r;
  int fd;

  if (!tool_arg(in, n, "path", &p)) return 0;
  if ((fd = open(p.s, O_RDONLY | O_CLOEXEC | O_NONBLOCK)) == -1) return 0;
  r = read(fd, b, sizeof b);
  close(fd);
  return r > 0 && media_type(b, (size_t)r) != 0;
}

/*
 * may this call run alongside others?  not if a rule denies it or a
 * hook is to run before it, as those must be dealt with in order.
 */
int
canpar(const struct tool *t, const char *in, size_t n)
{
  const struct rule *ru;
  unsigned long i;

  if (!t || !t->name || t->ask || !json_object(in, n)) return 0;
  for (i = 0; free_[i] && str_diff(free_[i], t->name); ++i) ;
  if (!free_[i]) return 0;
  if (!str_diff(t->name, "task") && inchild) return 0;
  if (!str_diff(t->name, "read_file") && media(in, n)) return 0;
  if (ruled(t, in, n, &ru) < 0) return 0;
  for (i = 0; i < nhooks; ++i)
    if (hooks[i].when == HOOK_BEFORE && (!fnmatch(hooks[i].tool, t->name, 0) ||
        (!str_diff(hooks[i].tool, "edit") && isedit(t->name))))
      return 0;
  return 1;
}

