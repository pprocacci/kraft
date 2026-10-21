#include "byte.h"
#include "edit.h"
#include "stralloc.h"

/* the index of the next occurrence of t in s at or after i, or n */
static size_t
find(const char *s, size_t n, size_t i, const char *t, size_t tn)
{
  for (; i + tn <= n; ++i)
    if (s[i] == t[0] && !byte_diff(s + i, tn, t)) return i;
  return n;
}

/*
 * replaces old with new in f: the one occurrence, or every one if all
 * is set.  refuses, leaving f alone and saying why in err, if old is
 * empty, the same as new, missing, or (without all) not unique.
 * returns the number of replacements, or 0.
 */
unsigned long
edit_apply(stralloc *f, const stralloc *old, const stralloc *new, int all,
    stralloc *err)
{
  stralloc g = STRALLOC_ZERO;
  unsigned long count = 0;
  size_t i, j;

  if (!old->len) {
    stralloc_copys(err, "old_string is empty; use write_file to create a file");
    return 0;
  }
  if (old->len == new->len && !byte_diff(old->s, old->len, new->s)) {
    stralloc_copys(err, "old_string and new_string are the same");
    return 0;
  }
  for (i = 0; (i = find(f->s, f->len, i, old->s, old->len)) < f->len;
      i += old->len)
    ++count;
  if (!count) {
    stralloc_copys(err, "old_string was not found; read the file again "
        "and copy the text exactly");
    return 0;
  }
  if (count > 1 && !all) {
    stralloc_copys(err, "old_string occurs ");
    stralloc_catulong(err, count);
    stralloc_cats(err, " times; add surrounding lines to make it unique, "
        "or set replace_all");
    return 0;
  }
  for (i = 0; (j = find(f->s, f->len, i, old->s, old->len)) < f->len;
      i = j + old->len)
    if (!stralloc_catb(&g, f->s + i, j - i) ||
        !stralloc_catb(&g, new->s, new->len))
      goto nomem;
  if (!stralloc_catb(&g, f->s + i, f->len - i)) goto nomem;
  if (!stralloc_copy(f, &g)) goto nomem;
  stralloc_free(&g);
  return count;
nomem:
  stralloc_free(&g);
  stralloc_copys(err, "out of memory");
  return 0;
}
