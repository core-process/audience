#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

bool audience_inner_init()
{
  return true;
}

void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url)
{
  return nullptr;
}

void audience_inner_window_destroy(void *window)
{
}

void audience_inner_loop()
{
}
