#include "anth.h"
#include "api.h"
#include "byte.h"
#include "json.h"
#include "str.h"
#include "stralloc.h"

static int
is(const char *v, size_t n, const char *s)
{
  size_t k = str_len(s);

  return n == k + 2 && v[0] == '"' && !byte_diff(v + 1, k, s);
}

/*
 * handles the JSON data of one Anthropic stream event.  appends any
 * reply text to text and any reasoning to think.  returns API_MORE,
 * API_DONE at message_stop, or API_ERROR with the message in text.
 */
int
anth_delta(const char *d, size_t n, stralloc *text, stralloc *think)
{
  const char *v;
  size_t vn;

  if (!json_get(d, n, "type", &v, &vn)) goto bad;
  if (is(v, vn, "content_block_delta")) {
    if (json_get(d, n, "delta.text", &v, &vn))
      return json_unstr(text, v, vn) ? API_MORE : API_ERROR;
    if (json_get(d, n, "delta.thinking", &v, &vn))
      return json_unstr(think, v, vn) ? API_MORE : API_ERROR;
    return API_MORE;
  }
  if (is(v, vn, "message_stop")) return API_DONE;
  if (is(v, vn, "error")) {
    if (json_get(d, n, "error.message", &v, &vn) && json_unstr(text, v, vn))
      return API_ERROR;
    goto bad;
  }
  return API_MORE;
bad:
  stralloc_cats(text, "malformed stream event");
  return API_ERROR;
}
