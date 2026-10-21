#include "anth.h"
#include "json.h"
#include "str.h"
#include "stralloc.h"

/* builds a streaming Anthropic Messages request with one user turn */
int
anth_body(stralloc *out, const char *model, unsigned long max,
    const char *prompt, size_t len)
{
  if (!stralloc_copys(out, "{\"model\":")) return 0;
  if (!json_str(out, model, str_len(model))) return 0;
  if (!stralloc_cats(out, ",\"max_tokens\":")) return 0;
  if (!stralloc_catulong(out, max)) return 0;
  if (!stralloc_cats(out, ",\"stream\":true,\"messages\":"
      "[{\"role\":\"user\",\"content\":"))
    return 0;
  if (!json_str(out, prompt, len)) return 0;
  return stralloc_cats(out, "}]}");
}
