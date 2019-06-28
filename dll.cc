#include <xlib.h>

#pragma comment(lib, "xlib")

#include "syringes.h"

using std::string;
using std::wstring;

#pragma pack (push, 1)
struct SyringesInfoST
  {
  DWORD pid;
  union
    {
    HANDLE hModule;
    char filename[MAX_PATH];
    };
  };
#pragma pack (pop)

static bool GetInfo(SyringesInfoST& si, sockaddr_in& addr)
  {
  xWSA wsa;
  SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
  if(sock == INVALID_SOCKET)
    {
    slog() << "socket fail : " << (int32)WSAGetLastError();
    return false;
    }
  auto s = AddrInfo(INADDR_ANY, gk_bindport);
  if(bind(sock, (const sockaddr*)&s, sizeof(s)))
    {
    slog() << "sock bind fail : " << (int32)WSAGetLastError();
    shutdown(sock, SD_BOTH);
    closesocket(sock);
    return false;
    }
  if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&gk_sock_timeout, sizeof(gk_sock_timeout)))
    {
    slog() << "sock set timeout fail : " << (int32)WSAGetLastError();
    shutdown(sock, SD_BOTH);
    closesocket(sock);
    return false;
    }

  memset(&si, 0, sizeof(si));
  
  int addrlen = sizeof(addr);
  memset(&addr, 0, sizeof(addr));

  // 如果不是由 syringes.exe 主动注入 DLL ，则 event 对象不存在。则不需要等待。
  auto event = OpenEvent(EVENT_ALL_ACCESS, FALSE, gk_event_name);
  if(nullptr != event)
    {
    // syringes.exe 建立好 Event 后即进入等待，所以这个先触发 syringes.exe 的等待，而不是后面的等待。
    SetEvent(event);
    slog() << "wait event...";
    const auto waitret = WaitForSingleObject(event, gk_thread_wait);
    if(waitret == WAIT_TIMEOUT)
      {
      slog() << "WaitForSingleObject timeout.";
      }
    if(waitret == WAIT_FAILED || waitret != WAIT_OBJECT_0)
      {
      slog() << "WaitForSingleObject fail : " << (uint32)waitret << " : " << (uint32)GetLastError();
      }
    CloseHandle(event);
    // 不管是否成功，都继续。
    }

  slog() << "waiting for syringes info...";
  const int recvlen = recvfrom(sock, (char*)&si, sizeof(si), 0, (sockaddr*)&addr, &addrlen);
  if(recvlen == 0)
    {
    slog() << "socket closed.";
    }
  else if(recvlen == SOCKET_ERROR)
    {
    slog() << "sock recv fail : " << (int32)WSAGetLastError();
    }
  else
    {
    sendto(sock, "ok", 2, 0, (const sockaddr*)&addr, sizeof(addr));
    }
  shutdown(sock, SD_BOTH);
  closesocket(sock);

  if(recvlen < (int)sizeof(si.pid))
    {
    if(recvlen > 0)
      {
      slog() << "\r\nrecv error data :\r\n" << showbin((const char*)&si, recvlen);
      }
    return false;
    }
  *((char*)&si + recvlen) = '\0';
  slog() << "\r\nrecvfrom : " << IpString(addr) << "\r\n" << showbin((const char*)&si, recvlen + 1);
  slog() << "    pid       : " << (uint32)si.pid;
  slog() << "    hModule   : " << si.hModule;
  slog() << "    filename  : " << si.filename;
  return true;
  }

bool UpperToken();
HANDLE LoadDll(HANDLE hProcess, const char* filename);
NTSTATUS UnloadDll(HANDLE hProcess, HANDLE hModule);

static void syringes()
  {
  SyringesInfoST si;
  sockaddr_in addr;
  if(!GetInfo(si, addr))  return;

  UpperToken();   // 无论是否提权成功，都尝试继续。

  slog() << "OpenProcess " << (uint32)si.pid << " ...";
  HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE, 0, si.pid);
	if(hProcess == nullptr)
		{
    slog() << "OpenProcess fail : " << (int32)GetLastError();
		return;
		}
  slog() << "OpenProcess " << (uint32)si.pid << " : " << hProcess;

  line nline;
  if(si.filename[0] != '\0')
    {
    nline << LoadDll(hProcess, si.filename);
    }
  else
    {
    nline << UnloadDll(hProcess, si.hModule);
    }
    
  slog() << "return to : " << IpString(addr);
  SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
  sendto(sock, (const char*)nline.c_str(), (int)nline.size(), 0, (sockaddr*)&addr, sizeof(addr));
  shutdown(sock, SD_BOTH);
  closesocket(sock);
  
  CloseHandle(hProcess);
  }


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
  {
  UNREFERENCED_PARAMETER(hModule);
  UNREFERENCED_PARAMETER(lpReserved);
  switch(ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
      slog() << "syringes dll start...";
      syringes();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      slog() << "syringes dll done.";
      break;
    }
  return FALSE;
  }
