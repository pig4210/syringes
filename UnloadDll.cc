#include <xlib.h>

#pragma comment(lib, "xlib")

#include "syringes.h"

static void* Get_NTDLL_Proc(__in LPCSTR lpProcName)
  {
  static const HMODULE gk_hmod_ntdll = GetModuleHandle(TEXT("ntdll"));
  return GetProcAddress(gk_hmod_ntdll, lpProcName);
  }

NTSTATUS UnloadDll(HANDLE hProcess, HANDLE hModule)
  {
  slog() << __FUNCTION__ "...";

  auto LdrUnloadDll = Get_NTDLL_Proc("LdrUnloadDll");
  if(LdrUnloadDll == nullptr)
    {
    slog() << "GetProcAddress LdrUnloadDll fail.";
    return -1;
    }

  slog() << "CreateRemoteThread";
	HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)(LdrUnloadDll), hModule, 0, nullptr);
	if(hThread == nullptr)
		{
    slog() << "CreateRemoteThread fail : " << (uint32)GetLastError();
    return -1;
		}

  slog() << "WaitForSingleObject...";
  const auto waitret = WaitForSingleObject(hThread, gk_thread_wait);
  if(waitret == WAIT_TIMEOUT)
    {
    slog() << "WaitForSingleObject timeout.";
	  CloseHandle(hThread);
    return -1;
    }
  if(waitret == WAIT_FAILED || waitret != WAIT_OBJECT_0)
    {
    slog() << "WaitForSingleObject fail : " << (uint32)waitret << " : " << (uint32)GetLastError();
	  CloseHandle(hThread);
    return -1;
    }
  DWORD ret;
  if(FALSE == GetExitCodeThread(hThread, &ret))
    {
    slog() << "GetExitCodeThread fail : " << (uint32)GetLastError();
    return -1;
    }
  CloseHandle(hThread);

  slog() << "NTSTATUS : " << (uint32)ret;
  return ret;
  }