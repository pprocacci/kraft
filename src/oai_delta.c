#include "api.h"
#include "byte.h"
#include "json.h"
#include "oai.h"
#include "stralloc.h"

/* appends the string at path, if there is one; 0 if it is malformed */
static int
take(const char *d, size_t n, const char *path, stralloc *out)
{
  const char *v;
  size_t vn;

  if (!json_get(d, n, path, &v, &vn) || *v != '"') return 1;
  return json_unstr(out, v, vn);
}

/*
 * handles the data of one Chat Completions stream chunk.  appends
 * reply text to text and reasoning (reasoning_content, as sent by
 * LM Studio, llama.cpp and DeepSeek, or reasoning) to think.
 * returns API_MORE, API_DONE at [DONE], or API_ERROR with the
 * message in text.
 */
int
oai_delta(const char *d, size_t n, stralloc *text, stralloc *think)
{
  const char *v;
  size_t vn;

  if (n == 6 && !byte_diff(d, 6, "[DONE]")) return API_DONE;
  if (json_get(d, n, "error", &v, &vn)) {
    if (!json_get(d, n, "error.message", &v, &vn) ||
        !json_unstr(text, v, vn))
      stralloc_cats(text, "server reported an error");
    return API_ERROR;
  }
  if (!json_get(d, n, "choices", &v, &vn)) {
    stralloc_cats(text, "malformed stream chunk");
    return API_ERROR;
  }
  if (!take(d, n, "choices.0.delta.content", text)) return API_ERROR;
  if (!take(d, n, "choices.0.delta.reasoning_content", think))
    return API_ERROR;
  if (!take(d, n, "choices.0.delta.reasoning", think)) return API_ERROR;
  return API_MORE;
}
