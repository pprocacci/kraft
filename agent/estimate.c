#include "agent.h"

/*
 * the conversation's size in tokens: what the server counted for the
 * last request and its reply, plus a rough four bytes a token for
 * what has been added since.
 */
unsigned long
estimate(void)
{
  return lastmain.in + lastmain.cread + lastmain.cwrite + lastmain.out +
      pending / 4;
}
