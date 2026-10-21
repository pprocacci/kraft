#include "agent.h"

/*
 * Anthropic's list prices, in cents per million tokens: input, output,
 * and a cache read.  A cache write is 1.25 times input (the
 * five-minute cache, which is what kraft uses).  Prices change: these
 * are as of the models' release.
 */
static const struct {
  const char *model;
  unsigned long in, out, cread;
} prices[] = {
  { "claude-fable-5-1", 1000, 5000, 25 },
  { "claude-mythos-5-1", 1000, 5000, 100 },
  { "claude-fable-5", 1000, 5000, 100 },
  { "claude-mythos-5", 1000, 5000, 100 },
  { "claude-opus-5-5", 400, 2000, 20 },
  { "claude-opus-5", 500, 2500, 50 },
  { "claude-opus-4-8", 500, 2500, 50 },
  { "claude-opus-4-7", 500, 2500, 50 },
  { "claude-opus-4-6", 500, 2500, 50 },
  { "claude-sonnet-5", 200, 1000, 20 },
  { "claude-sonnet-4-6", 300, 1500, 30 },
  { "claude-haiku-4-5", 100, 500, 10 },
};

/*
 * the cost of usage u on model m, in units of 10^-8 dollars (a
 * millionth of a cent), added to *cost.  returns 0 if m's price is
 * not known, or the server is not Anthropic's.
 */
int
price(const char *m, const struct usage *u, unsigned long long *cost)
{
  size_t i;

  if (!pdfok) return 0;  /* only the Anthropic API itself has these prices */
  for (i = 0; i < sizeof prices / sizeof prices[0]; ++i)
    if (!str_diff(m, prices[i].model)) {
      *cost += (unsigned long long)u->in * prices[i].in +
          (unsigned long long)u->cwrite * prices[i].in * 5 / 4 +
          (unsigned long long)u->cread * prices[i].cread +
          (unsigned long long)u->out * prices[i].out;
      return 1;
    }
  return 0;
}
