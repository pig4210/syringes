#include <xlib.h>

#pragma comment(lib, "xlib")

#include "syringes.h"

bool UpperToken()
	{
  slog() << __FUNCTION__ "...";
	HANDLE hToken;
	if(FALSE == OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
    slog() << "OpenProcessToken fail : " << (uint32)GetLastError();
    return false;
    }
  LUID luid;
  if(FALSE == LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid))
    {
    slog() << "LookupPrivilegeValue fail : " << (uint32)GetLastError();
    CloseHandle(hToken);
    return false;
    }
  TOKEN_PRIVILEGES tkp;
  tkp.PrivilegeCount = 1;
  tkp.Privileges[0].Luid = luid;
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if(FALSE == AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), nullptr, nullptr))
    {
    slog() << "AdjustTokenPrivileges fail : " << (uint32)GetLastError();
    CloseHandle(hToken);
    return false;
    }
  CloseHandle(hToken);
  slog() << __FUNCTION__ " done.";
  return true;
	}