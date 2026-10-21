#include "agent.h"

  /* the server takes PDFs */

/*
 * attaches the file at path to message m as a media block belonging
 * to tool call id (or to none, if id is empty), if it is an image, or
 * a PDF the server takes.  returns 1 if it did, 0 if the file is not
 * such a thing, -1 if it is but cannot be attached, with the reason
 * in why.  on success why says what was attached.
 */
int
attach(struct msg *m, const stralloc *id, const char *path, stralloc *why)
{
  stralloc f = STRALLOC_ZERO;
  struct block *b;
  const char *type;
  int r = 0;

  if (path_secret(path) || !file_get(path, &f)) goto out;
  if (!(type = media_type(f.s, f.len))) goto out;
  if (!str_diff(type, "application/pdf") && !pdfok) goto out;
  r = -1;
  if (f.len > (str_start(type, "image/") ? MAXIMAGE : MAXPDF)) {
    if (!stralloc_copys(why, path) || !stralloc_cats(why, " is too large "
        "to send to the model"))
      nomem();
    goto out;
  }
  if (!(b = conv_block(m, BLK_MEDIA))) nomem();
  if (!stralloc_copy(&b->id, id) || !stralloc_copys(&b->aux, type) ||
      !base64_enc(&b->text, f.s, f.len))
    nomem();
  if (!stralloc_copys(why, "attached ") || !stralloc_cats(why, path) ||
      !stralloc_cats(why, " (") || !stralloc_cats(why, type) ||
      !stralloc_cats(why, ", ") ||
      !stralloc_catulong(why, (unsigned long)(f.len + 1023) / 1024) ||
      !stralloc_cats(why, " KB) for the model to see"))
    nomem();
  /* an image is some 1600 tokens; a PDF page more, its size a guess */
  pending += str_start(type, "image/") ? 6400 : f.len / 2;
  r = 1;
out:
  stralloc_free(&f);
  return r;
}
