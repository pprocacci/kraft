#include "agent.h"

int
isedit(const char *name)
{
  return !str_diff(name, "write_file") || !str_diff(name, "edit_file") ||
      !str_diff(name, "multi_edit");
}
