#include <xlib.h>

#pragma comment(lib, "xlib")

#include "syringes.h"

struct LoadSyringesST
  {
  HANDLE hModule;
  UNICODE_STRING filename;
  decltype(&LdrLoadDll) _LdrLoadDll;
  NTSTATUS ret;
  };

DWORD __stdcall load_shellcode(LoadSyringesST* lsst)
  {
  lsst->ret = lsst->_LdrLoadDll(nullptr, 0, &lsst->filename, &lsst->hModule);
  return 0;
  }

void load_shellcode_end()
  {
  }

static void* Get_NTDLL_Proc(__in LPCSTR lpProcName)
  {
  static const HMODULE gk_hmod_ntdll = GetModuleHandle(TEXT("ntdll"));
  return GetProcAddress(gk_hmod_ntdll, lpProcName);
  }

HANDLE LoadDll(HANDLE hProcess, const char* filename)
  {
  slog() << __FUNCTION__ "...";

  if(filename == nullptr)
    {
    slog() << "filename nullptr.";
    return nullptr;
    }
  if(*filename == '\0')
    {
    slog() << "filename empty.";
    return nullptr;
    }

  LoadSyringesST lsst;

  lsst.hModule = nullptr;
  lsst.ret = 0;

  lsst._LdrLoadDll = (decltype(lsst._LdrLoadDll))Get_NTDLL_Proc("LdrLoadDll");
  if(lsst._LdrLoadDll == nullptr)
    {
    slog() << "GetProcAddress LdrLoadDll fail.";
    return nullptr;
    }

  const auto ws(s2ws(std::string(filename)));

  const size_t shellcodesize = (const uint8*)&load_shellcode_end - (const uint8*)&load_shellcode;
  const size_t filenamesize = (ws.size() + 1) * sizeof(wchar_t);
  const size_t need = sizeof(lsst) + filenamesize + shellcodesize;
  slog() << __FUNCTION__
    "  shellcode : " << shellcodesize <<
    "  filename : " << filenamesize <<
    "  need : " << need;

  lsst.filename.Length = (USHORT)(ws.size() * sizeof(wchar_t));
  lsst.filename.MaximumLength = (USHORT)filenamesize;
  
  slog() << "VirtualAllocEx...";
	void* mem = VirtualAllocEx(hProcess, nullptr, need > 1024 ? need : 1024, MEM_COMMIT,PAGE_EXECUTE_READWRITE);
	if(mem == nullptr)
		{
    slog() << "VirtualAllocEx fail : " << (uint32)GetLastError();
		return nullptr;
		}
  slog() << "VirtualAllocEx : " << mem;
  lsst.filename.Buffer = (PWCH)((const uint8*)mem + sizeof(lsst));
  
  std::string shellcode;
  shellcode.append((const char*)&lsst, sizeof(lsst));
  shellcode.append((const char*)ws.c_str(), filenamesize);
  shellcode.append((const char*)&load_shellcode, shellcodesize);

  slog() << "\r\nshellcode :\r\n" << showbin(shellcode.c_str(), shellcode.size());

  slog() << "WriteProcessMemory...";
  if(FALSE == WriteProcessMemory(hProcess, mem, shellcode.c_str(), shellcode.size(), nullptr))
    {
    slog() << "WriteProcessMemory fail : " << (uint32)GetLastError();
    VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return nullptr;
    }

  slog() << "CreateRemoteThread...";
	HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)((const uint8*)mem + sizeof(lsst) + filenamesize), mem, 0, nullptr);
	if(hThread == nullptr)
		{
    slog() << "CreateRemoteThread fail : " << (uint32)GetLastError();
    VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return nullptr;
		}

  slog() << "WaitForSingleObject...";
  const auto waitret = WaitForSingleObject(hThread, gk_thread_wait);
  if(waitret == WAIT_TIMEOUT)
    {
    slog() << "WaitForSingleObject timeout.";
	  CloseHandle(hThread);
		VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return nullptr;
    }
  if(waitret == WAIT_FAILED || waitret != WAIT_OBJECT_0)
    {
    slog() << "WaitForSingleObject fail : " << (uint32)waitret << " : " << (uint32)GetLastError();
	  CloseHandle(hThread);
		VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return nullptr;
    }
  CloseHandle(hThread);

  slog() << "ReadProcessMemory...";
  if(FALSE == ReadProcessMemory(hProcess, mem, &lsst, sizeof(lsst), nullptr))
    {
    slog() << "ReadProcessMemory fail : " << (uint32)GetLastError();
    VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return nullptr;
    }
  VirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);

  slog() << "NTSTATUS : " << (uint32)lsst.ret;
  slog() << "HMODLUE  : " << lsst.hModule;
  return NT_SUCCESS(lsst.ret) ? lsst.hModule : nullptr;
  }