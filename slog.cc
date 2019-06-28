#include <xlib.h>

#pragma comment(lib, "xlib")

#include "syringes.h"

static void dbgout(const char* msg)
  {
  OutputDebugStringA(msg);
  }

slog::~slog()
  {
  if(empty())  return;
  out(c_str());
  clear();
  }

decltype(&dbgout) slog::out = dbgout;