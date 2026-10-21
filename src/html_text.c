#include "byte.h"
#include "html.h"
#include "str.h"
#include "stralloc.h"

/* elements whose contents are not text to read */
static const char *const skipped[] = {
  "script", "style", "noscript", "svg", "template", "head", 0
};

/* elements that start a new line */
static const char *const lines[] = {
  "br", "li", "tr", "dt", "dd", "option", 0
};

/* elements set off by a blank line */
static const char *const blocks[] = {
  "p", "div", "ul", "ol", "dl", "h1", "h2", "h3", "h4", "h5", "h6",
  "table", "section", "article", "header", "footer", "nav", "aside",
  "main", "blockquote", "hr", "pre", "form", "figure", "figcaption",
  "title", "body", 0
};

struct out {
  stralloc *sa;
  int nl;      /* newlines just written */
  int space;   /* a space is due before the next text */
  int ok;
};

static int
among(const char *w, const char *const *list)
{
  for (; *list; ++list)
    if (!str_diff(w, *list)) return 1;
  return 0;
}

static void
put(struct out *o, const char *s, size_t n)
{
  if (o->space && !o->nl && o->sa->len && o->sa->s[o->sa->len - 1] != ' ') {
    if (!stralloc_append(o->sa, ' ')) o->ok = 0;
  }
  o->space = 0;
  if (!stralloc_catb(o->sa, s, n)) o->ok = 0;
  o->nl = 0;
}

/* ends the line: want is 1 for a new line, 2 for a blank line too */
static void
newline(struct out *o, int want)
{
  o->space = 0;
  if (!o->sa->len) return;
  while (o->sa->len && o->sa->s[o->sa->len - 1] == ' ' && !o->nl)
    --o->sa->len;  /* no trailing spaces */
  while (o->nl < want) {
    if (!stralloc_append(o->sa, '\n')) { o->ok = 0; return; }
    ++o->nl;
  }
}

static void
utf8(struct out *o, unsigned long u)
{
  char b[4];
  size_t n;

  if (!u || u > 0x10ffff || (u >= 0xd800 && u < 0xe000)) u = 0xfffd;
  if (u < 0x80) { b[0] = (char)u; n = 1; }
  else if (u < 0x800) {
    b[0] = (char)(0xc0 | (u >> 6));
    b[1] = (char)(0x80 | (u & 0x3f));
    n = 2;
  } else if (u < 0x10000) {
    b[0] = (char)(0xe0 | (u >> 12));
    b[1] = (char)(0x80 | ((u >> 6) & 0x3f));
    b[2] = (char)(0x80 | (u & 0x3f));
    n = 3;
  } else {
    b[0] = (char)(0xf0 | (u >> 18));
    b[1] = (char)(0x80 | ((u >> 12) & 0x3f));
    b[2] = (char)(0x80 | ((u >> 6) & 0x3f));
    b[3] = (char)(0x80 | (u & 0x3f));
    n = 4;
  }
  put(o, b, n);
}

/* an entity at s[0] == '&'; returns the bytes used, 0 if it is not one */
static size_t
entity(struct out *o, const char *s, size_t n)
{
  static const struct { const char *name; const char *text; } named[] = {
    { "amp", "&" }, { "lt", "<" }, { "gt", ">" }, { "quot", "\"" },
    { "apos", "'" }, { "nbsp", " " }, { "copy", "\xc2\xa9" },
    { "mdash", "\xe2\x80\x94" }, { "ndash", "\xe2\x80\x93" },
    { "hellip", "\xe2\x80\xa6" }, { "rsquo", "\xe2\x80\x99" },
    { "lsquo", "\xe2\x80\x98" }, { "rdquo", "\xe2\x80\x9d" },
    { "ldquo", "\xe2\x80\x9c" }, { 0, 0 }
  };
  unsigned long u = 0;
  size_t e, i, k;
  int hex;

  for (e = 1; e < n && e < 12 && s[e] != ';'; ++e) ;
  if (e >= n || s[e] != ';') return 0;
  if (s[1] == '#') {
    hex = s[2] == 'x' || s[2] == 'X';
    for (i = hex ? 3 : 2; i < e; ++i) {
      if (s[i] >= '0' && s[i] <= '9') u = u * (hex ? 16 : 10) + (unsigned long)(s[i] - '0');
      else if (hex && s[i] >= 'a' && s[i] <= 'f') u = u * 16 + (unsigned long)(s[i] - 'a' + 10);
      else if (hex && s[i] >= 'A' && s[i] <= 'F') u = u * 16 + (unsigned long)(s[i] - 'A' + 10);
      else return 0;
      if (u > 0x10ffff) u = 0x110000;
    }
    if (i == (size_t)(hex ? 3 : 2)) return 0;
    utf8(o, u);
    return e + 1;
  }
  for (k = 0; named[k].name; ++k)
    if (str_len(named[k].name) == e - 1 &&
        !byte_diff(s + 1, e - 1, named[k].name)) {
      if (named[k].text[0] == ' ' && !named[k].text[1]) o->space = 1;
      else put(o, named[k].text, str_len(named[k].text));
      return e + 1;
    }
  return 0;
}

/* the end of the tag starting at s[i] == '<', past its '>' */
static size_t
tagend(const char *s, size_t n, size_t i)
{
  char q = 0;

  for (++i; i < n; ++i) {
    if (q) { if (s[i] == q) q = 0; }
    else if (s[i] == '"' || s[i] == '\'') q = s[i];
    else if (s[i] == '>') return i + 1;
  }
  return n;
}

/* the index of "</name" (any case) at or after i, or n */
static size_t
closing(const char *s, size_t n, size_t i, const char *name)
{
  size_t k = str_len(name), m;
  char c;

  for (; i + 2 + k <= n; ++i) {
    if (s[i] != '<' || s[i + 1] != '/') continue;
    for (m = 0; m < k; ++m) {
      c = s[i + 2 + m];
      if (c >= 'A' && c <= 'Z') c += 32;
      if (c != name[m]) break;
    }
    if (m == k) return i;
  }
  return n;
}

/*
 * appends the readable text of the HTML in s to out: no markup,
 * scripts or styles; entities decoded; block elements on lines of
 * their own and list items marked "- "; spaces collapsed except in
 * <pre>.  returns 1, or 0 if memory runs out.
 */
int
html_text(stralloc *sa, const char *s, size_t n)
{
  struct out o;
  char name[16];
  size_t i = 0, j, k, e;
  int close, pre = 0;

  o.sa = sa;
  o.nl = 2;
  o.space = 0;
  o.ok = 1;
  while (i < n && o.ok) {
    if (s[i] == '<') {
      if (n - i >= 4 && !byte_diff(s + i, 4, "<!--")) {
        for (j = i + 4; j + 3 <= n && byte_diff(s + j, 3, "-->"); ++j) ;
        i = j + 3 <= n ? j + 3 : n;
        continue;
      }
      j = i + 1;
      close = j < n && s[j] == '/';
      if (close) ++j;
      for (k = 0; j < n && k < sizeof name - 1 &&
          ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') ||
          (s[j] >= '0' && s[j] <= '9')); ++j)
        name[k++] = (s[j] >= 'A' && s[j] <= 'Z') ? s[j] + 32 : s[j];
      name[k] = 0;
      if (!k && !(j < n && (s[j] == '!' || s[j] == '?'))) {
        put(&o, "<", 1);  /* a lone < in text */
        ++i;
        continue;
      }
      e = tagend(s, n, i);
      if (!close && among(name, skipped)) {
        j = closing(s, n, e, name);
        i = j < n ? tagend(s, n, j) : n;
        continue;
      }
      if (!str_diff(name, "pre")) pre = !close;
      if (among(name, lines)) {
        if (!close || str_diff(name, "li")) newline(&o, 1);
        if (!close && !str_diff(name, "li")) put(&o, "- ", 2);
      } else if (among(name, blocks))
        newline(&o, 2);
      else if (!str_diff(name, "td") || !str_diff(name, "th"))
        o.space = 1;
      i = e;
      continue;
    }
    if (s[i] == '&' && (k = entity(&o, s + i, n - i))) {
      i += k;
      continue;
    }
    if (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') {
      if (pre) {
        if (s[i] == '\n') {
          if (!stralloc_append(sa, '\n')) o.ok = 0;
          o.nl = 1;
        } else if (s[i] != '\r') put(&o, s + i, 1);
      } else
        o.space = 1;
      ++i;
      continue;
    }
    put(&o, s + i, 1);
    ++i;
  }
  return o.ok;
}
