#include "agent.h"

void
save(void)
{
  stralloc s = STRALLOC_ZERO;
  int fd;

  if (!sessfn.len || !conv.n) return;
  if (!stralloc_copys(&s, "{\"kraft\":1,\"cwd\":") ||
      !json_str(&s, cwdsa.s, str_len(cwdsa.s)) ||
      !stralloc_cats(&s, ",\"api\":") ||
      !json_str(&s, api->name, str_len(api->name)) ||
      !stralloc_cats(&s, ",\"model\":") ||
      !json_str(&s, model, str_len(model)) || !stralloc_cats(&s, "}\n") ||
      !conv_save(&s, &conv))
    nomem();
  /* a new file is made private first; file_put keeps the mode */
  fd = open(sessfn.s, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd != -1) close(fd);
  if (!file_put(sessfn.s, s.s, s.len) && !savewarned) {
    kwarnsys("unable to save the session to ", sessfn.s);
    savewarned = 1;
  }
  stralloc_free(&s);
}
