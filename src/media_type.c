#include "byte.h"
#include "media.h"

/*
 * the media type of an image or PDF, judged by its first bytes, or 0
 * if it is neither.  these are the types the model APIs accept.
 */
const char *
media_type(const char *s, size_t n)
{
  if (n >= 8 && !byte_diff(s, 8, "\211PNG\r\n\032\n")) return "image/png";
  if (n >= 3 && !byte_diff(s, 3, "\377\330\377")) return "image/jpeg";
  if (n >= 6 && (!byte_diff(s, 6, "GIF87a") || !byte_diff(s, 6, "GIF89a")))
    return "image/gif";
  if (n >= 12 && !byte_diff(s, 4, "RIFF") && !byte_diff(s + 8, 4, "WEBP"))
    return "image/webp";
  if (n >= 5 && !byte_diff(s, 5, "%PDF-")) return "application/pdf";
  return 0;
}
