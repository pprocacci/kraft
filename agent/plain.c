#include "agent.h"

/* no ; & | $( ` < > or newline: a single plain command */
int
plain(const char *s)
{
  for (; *s; ++s)
    if (*s == ';' || *s == '&' || *s == '|' || *s == '`' || *s == '<' ||
        *s == '>' || *s == '\n' || (*s == '$' && s[1] == '('))
      return 0;
  return 1;
}
