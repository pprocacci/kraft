#include "json.h"
#include "stralloc.h"

/* appends s as a quoted, escaped JSON string */
int
json_str(stralloc *out, const char *s, size_t n)
{
  static const char hex[] = "0123456789abcdef";
  unsigned char c;
  char u[6] = { '\\', 'u', '0', '0', 0, 0 };
  size_t i;

  if (!stralloc_append(out, '"')) return 0;
  for (i = 0; i < n; ++i) {
    c = (unsigned char)s[i];
    switch (c) {
    case '"': if (!stralloc_cats(out, "\\\"")) return 0; break;
    case '\\': if (!stralloc_cats(out, "\\\\")) return 0; break;
    case '\n': if (!stralloc_cats(out, "\\n")) return 0; break;
    case '\r': if (!stralloc_cats(out, "\\r")) return 0; break;
    case '\t': if (!stralloc_cats(out, "\\t")) return 0; break;
    default:
      if (c < 0x20 || c == 0x7f) {
        u[4] = hex[c >> 4];
        u[5] = hex[c & 15];
        if (!stralloc_catb(out, u, 6)) return 0;
      } else if (!stralloc_append(out, (char)c)) return 0;
    }
  }
  return stralloc_append(out, '"');
}
